/*
 * Copyright 2012 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "include/effects/SkMorphologyImageFilter.h"

#include "include/core/SkBitmap.h"
#include "include/core/SkRect.h"
#include "include/private/SkColorData.h"
#include "src/core/SkImageFilter_Base.h"
#include "src/core/SkReadBuffer.h"
#include "src/core/SkSpecialImage.h"
#include "src/core/SkWriteBuffer.h"

#if SK_SUPPORT_GPU
#include "include/gpu/GrRecordingContext.h"
#include "src/gpu/GrContextPriv.h"
#include "src/gpu/GrRecordingContextPriv.h"
#include "src/gpu/GrRenderTargetContext.h"
#include "src/gpu/GrTexture.h"
#include "src/gpu/GrTextureProxy.h"
#include "src/gpu/SkGr.h"
#include "src/gpu/glsl/GrGLSLFragmentProcessor.h"
#include "src/gpu/glsl/GrGLSLFragmentShaderBuilder.h"
#include "src/gpu/glsl/GrGLSLProgramDataManager.h"
#include "src/gpu/glsl/GrGLSLUniformHandler.h"
#endif

namespace {

enum class MorphType {
    kErode,
    kDilate,
    kLastType = kDilate
};

enum class MorphDirection { kX, kY };

class SkMorphologyImageFilterImpl final : public SkImageFilter_Base {
public:
    SkMorphologyImageFilterImpl(MorphType type, SkScalar radiusX, SkScalar radiusY,
                                sk_sp<SkImageFilter> input, const CropRect* cropRect)
            : INHERITED(&input, 1, cropRect)
            , fType(type)
            , fRadius(SkSize::Make(radiusX, radiusY)) {}

    SkRect computeFastBounds(const SkRect& src) const override;
    SkIRect onFilterNodeBounds(const SkIRect& src, const SkMatrix& ctm,
                               MapDirection, const SkIRect* inputRect) const override;

    /**
     * All morphology procs have the same signature: src is the source buffer, dst the
     * destination buffer, radius is the morphology radius, width and height are the bounds
     * of the destination buffer (in pixels), and srcStride and dstStride are the
     * number of pixels per row in each buffer. All buffers are 8888.
     */

    typedef void (*Proc)(const SkPMColor* src, SkPMColor* dst, int radius,
                         int width, int height, int srcStride, int dstStride);

protected:
    sk_sp<SkSpecialImage> onFilterImage(const Context&, SkIPoint* offset) const override;
    void flatten(SkWriteBuffer&) const override;

    SkSize mappedRadius(const SkMatrix& ctm) const {
      SkVector radiusVector = SkVector::Make(fRadius.width(), fRadius.height());
      ctm.mapVectors(&radiusVector, 1);
      radiusVector.setAbs(radiusVector);
      return SkSize::Make(radiusVector.x(), radiusVector.y());
    }

private:
    friend void SkDilateImageFilter::RegisterFlattenables();

    SK_FLATTENABLE_HOOKS(SkMorphologyImageFilterImpl)

    MorphType fType;
    SkSize    fRadius;

    typedef SkImageFilter_Base INHERITED;
};

} // end namespace

sk_sp<SkImageFilter> SkDilateImageFilter::Make(SkScalar radiusX, SkScalar radiusY,
                                               sk_sp<SkImageFilter> input,
                                               const SkImageFilter::CropRect* cropRect) {
    if (radiusX < 0 || radiusY < 0) {
        return nullptr;
    }
    return sk_sp<SkImageFilter>(new SkMorphologyImageFilterImpl(
            MorphType::kDilate, radiusX, radiusY, std::move(input), cropRect));
}

sk_sp<SkImageFilter> SkErodeImageFilter::Make(SkScalar radiusX, SkScalar radiusY,
                                              sk_sp<SkImageFilter> input,
                                              const SkImageFilter::CropRect* cropRect) {
    if (radiusX < 0 || radiusY < 0) {
        return nullptr;
    }
    return sk_sp<SkImageFilter>(new SkMorphologyImageFilterImpl(
            MorphType::kErode, radiusX, radiusY,  std::move(input), cropRect));
}

void SkDilateImageFilter::RegisterFlattenables() {
    SK_REGISTER_FLATTENABLE(SkMorphologyImageFilterImpl);
}

///////////////////////////////////////////////////////////////////////////////

sk_sp<SkFlattenable> SkMorphologyImageFilterImpl::CreateProc(SkReadBuffer& buffer) {
    SK_IMAGEFILTER_UNFLATTEN_COMMON(common, 1);
    SkScalar width;
    SkScalar height;
    if (buffer.isVersionLT(SkPicturePriv::kMorphologyTakesScalar_Version)) {
        width = buffer.readInt();
        height = buffer.readInt();
    } else {
        width = buffer.readScalar();
        height = buffer.readScalar();
    }

    MorphType filterType = buffer.read32LE(MorphType::kLastType);

    if (filterType == MorphType::kDilate) {
        return SkDilateImageFilter::Make(width, height, common.getInput(0), &common.cropRect());
    } else if (filterType == MorphType::kErode) {
        return SkErodeImageFilter::Make(width, height, common.getInput(0), &common.cropRect());
    } else {
        return nullptr;
    }
}

void SkMorphologyImageFilterImpl::flatten(SkWriteBuffer& buffer) const {
    this->INHERITED::flatten(buffer);
    buffer.writeScalar(fRadius.fWidth);
    buffer.writeScalar(fRadius.fHeight);
    buffer.writeInt(static_cast<int>(fType));
}

static void call_proc_X(SkMorphologyImageFilterImpl::Proc procX,
                        const SkBitmap& src, SkBitmap* dst,
                        int radiusX, const SkIRect& bounds) {
    procX(src.getAddr32(bounds.left(), bounds.top()), dst->getAddr32(0, 0),
          radiusX, bounds.width(), bounds.height(),
          src.rowBytesAsPixels(), dst->rowBytesAsPixels());
}

static void call_proc_Y(SkMorphologyImageFilterImpl::Proc procY,
                        const SkPMColor* src, int srcRowBytesAsPixels, SkBitmap* dst,
                        int radiusY, const SkIRect& bounds) {
    procY(src, dst->getAddr32(0, 0),
          radiusY, bounds.height(), bounds.width(),
          srcRowBytesAsPixels, dst->rowBytesAsPixels());
}

SkRect SkMorphologyImageFilterImpl::computeFastBounds(const SkRect& src) const {
    SkRect bounds = this->getInput(0) ? this->getInput(0)->computeFastBounds(src) : src;
    bounds.outset(fRadius.width(), fRadius.height());
    return bounds;
}

SkIRect SkMorphologyImageFilterImpl::onFilterNodeBounds(
        const SkIRect& src, const SkMatrix& ctm, MapDirection, const SkIRect* inputRect) const {
    SkSize radius = mappedRadius(ctm);
    return src.makeOutset(SkScalarCeilToInt(radius.width()), SkScalarCeilToInt(radius.height()));
}

#if SK_SUPPORT_GPU

///////////////////////////////////////////////////////////////////////////////
/**
 * Morphology effects. Depending upon the type of morphology, either the
 * component-wise min (Erode_Type) or max (Dilate_Type) of all pixels in the
 * kernel is selected as the new color. The new color is modulated by the input
 * color.
 */
class GrMorphologyEffect : public GrFragmentProcessor {
public:
    static std::unique_ptr<GrFragmentProcessor> Make(
            std::unique_ptr<GrFragmentProcessor> inputFP, GrSurfaceProxyView view,
            SkAlphaType srcAlphaType, MorphDirection dir, int radius, MorphType type) {
        return std::unique_ptr<GrFragmentProcessor>(
                new GrMorphologyEffect(std::move(inputFP), std::move(view), srcAlphaType, dir,
                                       radius, type, /*range=*/nullptr));
    }

    static std::unique_ptr<GrFragmentProcessor> Make(
            std::unique_ptr<GrFragmentProcessor> inputFP, GrSurfaceProxyView view,
            SkAlphaType srcAlphaType, MorphDirection dir, int radius, MorphType type,
            const float range[2]) {
        return std::unique_ptr<GrFragmentProcessor>(new GrMorphologyEffect(
                std::move(inputFP), std::move(view), srcAlphaType, dir, radius, type, range));
    }

    const char* name() const override { return "Morphology"; }

    std::unique_ptr<GrFragmentProcessor> clone() const override {
        return std::unique_ptr<GrFragmentProcessor>(new GrMorphologyEffect(*this));
    }

private:
    MorphDirection fDirection;
    int fRadius;
    MorphType fType;
    bool fUseRange;
    float fRange[2];

    GrGLSLFragmentProcessor* onCreateGLSLInstance() const override;

    void onGetGLSLProcessorKey(const GrShaderCaps&, GrProcessorKeyBuilder*) const override;

    bool onIsEqual(const GrFragmentProcessor&) const override;
    GrMorphologyEffect(std::unique_ptr<GrFragmentProcessor> inputFP, GrSurfaceProxyView,
                       SkAlphaType srcAlphaType, MorphDirection, int radius, MorphType,
                       const float range[2]);
    explicit GrMorphologyEffect(const GrMorphologyEffect&);

    GR_DECLARE_FRAGMENT_PROCESSOR_TEST

    typedef GrFragmentProcessor INHERITED;
};

GrGLSLFragmentProcessor* GrMorphologyEffect::onCreateGLSLInstance() const {
    class Impl : public GrGLSLFragmentProcessor {
    public:
        void emitCode(EmitArgs& args) override {
            constexpr int kInputFPIndex = 0;
            constexpr int kTexEffectIndex = 1;

            const GrMorphologyEffect& me = args.fFp.cast<GrMorphologyEffect>();

            GrGLSLUniformHandler* uniformHandler = args.fUniformHandler;
            fRangeUni = uniformHandler->addUniform(&me, kFragment_GrShaderFlag, kFloat2_GrSLType,
                                                   "Range");
            const char* range = uniformHandler->getUniformCStr(fRangeUni);

            GrGLSLFPFragmentBuilder* fragBuilder = args.fFragBuilder;

            const char* func = me.fType == MorphType::kErode ? "min" : "max";

            char initialValue = me.fType == MorphType::kErode ? '1' : '0';
            fragBuilder->codeAppendf("%s = half4(%c);", args.fOutputColor, initialValue);

            char dir = me.fDirection == MorphDirection::kX ? 'x' : 'y';

            int width = 2 * me.fRadius + 1;

            // float2 coord = coord2D;
            fragBuilder->codeAppendf("float2 coord = %s;", args.fSampleCoord);
            // coord.x -= radius;
            fragBuilder->codeAppendf("coord.%c -= %d;", dir, me.fRadius);
            if (me.fUseRange) {
                // highBound = min(highBound, coord.x + (width-1));
                fragBuilder->codeAppendf("float highBound = min(%s.y, coord.%c + %f);", range, dir,
                                         float(width - 1));
                // coord.x = max(lowBound, coord.x);
                fragBuilder->codeAppendf("coord.%c = max(%s.x, coord.%c);", dir, range, dir);
            }
            fragBuilder->codeAppendf("for (int i = 0; i < %d; i++) {", width);
            SkString sample = this->invokeChild(kTexEffectIndex, args, "coord");
            fragBuilder->codeAppendf("    %s = %s(%s, %s);", args.fOutputColor, func,
                                     args.fOutputColor, sample.c_str());
            // coord.x += 1;
            fragBuilder->codeAppendf("    coord.%c += 1;", dir);
            if (me.fUseRange) {
                // coord.x = min(highBound, coord.x);
                fragBuilder->codeAppendf("    coord.%c = min(highBound, coord.%c);", dir, dir);
            }
            fragBuilder->codeAppend("}");

            SkString inputColor = this->invokeChild(kInputFPIndex, args);
            fragBuilder->codeAppendf("%s *= %s;", args.fOutputColor, inputColor.c_str());
        }

    protected:
        void onSetData(const GrGLSLProgramDataManager& pdman,
                       const GrFragmentProcessor& proc) override {
            const GrMorphologyEffect& m = proc.cast<GrMorphologyEffect>();
            if (m.fUseRange) {
                pdman.set2f(fRangeUni, m.fRange[0], m.fRange[1]);
            }
        }

    private:
        GrGLSLProgramDataManager::UniformHandle fRangeUni;
    };
    return new Impl;
}

void GrMorphologyEffect::onGetGLSLProcessorKey(const GrShaderCaps& caps,
                                               GrProcessorKeyBuilder* b) const {
    uint32_t key = static_cast<uint32_t>(fRadius);
    key |= (static_cast<uint32_t>(fType) << 8);
    key |= (static_cast<uint32_t>(fDirection) << 9);
    if (fUseRange) {
        key |= 1 << 10;
    }
    b->add32(key);
}

GrMorphologyEffect::GrMorphologyEffect(std::unique_ptr<GrFragmentProcessor> inputFP,
                                       GrSurfaceProxyView view,
                                       SkAlphaType srcAlphaType,
                                       MorphDirection direction,
                                       int radius,
                                       MorphType type,
                                       const float range[2])
        : INHERITED(kGrMorphologyEffect_ClassID, ModulateForClampedSamplerOptFlags(srcAlphaType))
        , fDirection(direction)
        , fRadius(radius)
        , fType(type)
        , fUseRange(SkToBool(range)) {
    this->setUsesSampleCoordsDirectly();
    this->registerChild(std::move(inputFP));
    this->registerChild(GrTextureEffect::Make(std::move(view), srcAlphaType),
                        SkSL::SampleUsage::Explicit());
    if (fUseRange) {
        fRange[0] = range[0];
        fRange[1] = range[1];
    }
}

GrMorphologyEffect::GrMorphologyEffect(const GrMorphologyEffect& that)
        : INHERITED(kGrMorphologyEffect_ClassID, that.optimizationFlags())
        , fDirection(that.fDirection)
        , fRadius(that.fRadius)
        , fType(that.fType)
        , fUseRange(that.fUseRange) {
    this->setUsesSampleCoordsDirectly();
    this->cloneAndRegisterAllChildProcessors(that);
    if (that.fUseRange) {
        fRange[0] = that.fRange[0];
        fRange[1] = that.fRange[1];
    }
}

bool GrMorphologyEffect::onIsEqual(const GrFragmentProcessor& sBase) const {
    const GrMorphologyEffect& s = sBase.cast<GrMorphologyEffect>();
    return this->fRadius    == s.fRadius    &&
           this->fDirection == s.fDirection &&
           this->fUseRange  == s.fUseRange  &&
           this->fType      == s.fType;
}

///////////////////////////////////////////////////////////////////////////////

GR_DEFINE_FRAGMENT_PROCESSOR_TEST(GrMorphologyEffect);

#if GR_TEST_UTILS
std::unique_ptr<GrFragmentProcessor> GrMorphologyEffect::TestCreate(GrProcessorTestData* d) {
    auto [view, ct, at] = d->randomView();

    MorphDirection dir = d->fRandom->nextBool() ? MorphDirection::kX : MorphDirection::kY;
    static const int kMaxRadius = 10;
    int radius = d->fRandom->nextRangeU(1, kMaxRadius);
    MorphType type = d->fRandom->nextBool() ? MorphType::kErode : MorphType::kDilate;
    return GrMorphologyEffect::Make(d->inputFP(), std::move(view), at, dir, radius, type);
}
#endif

static void apply_morphology_rect(GrRenderTargetContext* renderTargetContext,
                                  GrSurfaceProxyView view,
                                  SkAlphaType srcAlphaType,
                                  const SkIRect& srcRect,
                                  const SkIRect& dstRect,
                                  int radius,
                                  MorphType morphType,
                                  const float range[2],
                                  MorphDirection direction) {
    GrPaint paint;
    paint.setColorFragmentProcessor(GrMorphologyEffect::Make(/*inputFP=*/nullptr, std::move(view),
                                                             srcAlphaType, direction, radius,
                                                             morphType, range));
    paint.setPorterDuffXPFactory(SkBlendMode::kSrc);
    renderTargetContext->fillRectToRect(/*clip=*/nullptr, std::move(paint), GrAA::kNo,
                                        SkMatrix::I(), SkRect::Make(dstRect),
                                        SkRect::Make(srcRect));
}

static void apply_morphology_rect_no_bounds(GrRenderTargetContext* renderTargetContext,
                                            GrSurfaceProxyView view,
                                            SkAlphaType srcAlphaType,
                                            const SkIRect& srcRect,
                                            const SkIRect& dstRect,
                                            int radius,
                                            MorphType morphType,
                                            MorphDirection direction) {
    GrPaint paint;
    paint.setColorFragmentProcessor(GrMorphologyEffect::Make(
            /*inputFP=*/nullptr, std::move(view), srcAlphaType, direction, radius, morphType));
    paint.setPorterDuffXPFactory(SkBlendMode::kSrc);
    renderTargetContext->fillRectToRect(/*clip=*/nullptr, std::move(paint), GrAA::kNo,
                                        SkMatrix::I(), SkRect::Make(dstRect),
                                        SkRect::Make(srcRect));
}

static void apply_morphology_pass(GrRenderTargetContext* renderTargetContext,
                                  GrSurfaceProxyView view,
                                  SkAlphaType srcAlphaType,
                                  const SkIRect& srcRect,
                                  const SkIRect& dstRect,
                                  int radius,
                                  MorphType morphType,
                                  MorphDirection direction) {
    float bounds[2] = { 0.0f, 1.0f };
    SkIRect lowerSrcRect = srcRect, lowerDstRect = dstRect;
    SkIRect middleSrcRect = srcRect, middleDstRect = dstRect;
    SkIRect upperSrcRect = srcRect, upperDstRect = dstRect;
    if (direction == MorphDirection::kX) {
        bounds[0] = SkIntToScalar(srcRect.left()) + 0.5f;
        bounds[1] = SkIntToScalar(srcRect.right()) - 0.5f;
        lowerSrcRect.fRight = srcRect.left() + radius;
        lowerDstRect.fRight = dstRect.left() + radius;
        upperSrcRect.fLeft = srcRect.right() - radius;
        upperDstRect.fLeft = dstRect.right() - radius;
        middleSrcRect.inset(radius, 0);
        middleDstRect.inset(radius, 0);
    } else {
        bounds[0] = SkIntToScalar(srcRect.top()) + 0.5f;
        bounds[1] = SkIntToScalar(srcRect.bottom()) - 0.5f;
        lowerSrcRect.fBottom = srcRect.top() + radius;
        lowerDstRect.fBottom = dstRect.top() + radius;
        upperSrcRect.fTop = srcRect.bottom() - radius;
        upperDstRect.fTop = dstRect.bottom() - radius;
        middleSrcRect.inset(0, radius);
        middleDstRect.inset(0, radius);
    }
    if (middleSrcRect.width() <= 0) {
        // radius covers srcRect; use bounds over entire draw
        apply_morphology_rect(renderTargetContext, std::move(view), srcAlphaType, srcRect,
                              dstRect, radius, morphType, bounds, direction);
    } else {
        // Draw upper and lower margins with bounds; middle without.
        apply_morphology_rect(renderTargetContext, view, srcAlphaType, lowerSrcRect,
                              lowerDstRect, radius, morphType, bounds, direction);
        apply_morphology_rect(renderTargetContext, view, srcAlphaType, upperSrcRect,
                              upperDstRect, radius, morphType, bounds, direction);
        apply_morphology_rect_no_bounds(renderTargetContext, std::move(view), srcAlphaType,
                                        middleSrcRect, middleDstRect, radius, morphType, direction);
    }
}

static sk_sp<SkSpecialImage> apply_morphology(
        GrRecordingContext* context, SkSpecialImage* input, const SkIRect& rect,
        MorphType morphType, SkISize radius, const SkImageFilter_Base::Context& ctx) {
    GrSurfaceProxyView srcView = input->view(context);
    SkAlphaType srcAlphaType = input->alphaType();
    SkASSERT(srcView.asTextureProxy());
    sk_sp<SkColorSpace> colorSpace = ctx.refColorSpace();
    GrColorType colorType = ctx.grColorType();

    GrSurfaceProxy* proxy = srcView.proxy();

    const SkIRect dstRect = SkIRect::MakeWH(rect.width(), rect.height());
    SkIRect srcRect = rect;
    // Map into proxy space
    srcRect.offset(input->subset().x(), input->subset().y());
    SkASSERT(radius.width() > 0 || radius.height() > 0);

    if (radius.fWidth > 0) {
        auto dstRTContext = GrRenderTargetContext::Make(
                context, colorType, colorSpace, SkBackingFit::kApprox, rect.size(), 1,
                GrMipmapped::kNo, proxy->isProtected(), kBottomLeft_GrSurfaceOrigin);
        if (!dstRTContext) {
            return nullptr;
        }

        apply_morphology_pass(dstRTContext.get(), std::move(srcView), srcAlphaType,
                              srcRect, dstRect, radius.fWidth, morphType, MorphDirection::kX);
        SkIRect clearRect = SkIRect::MakeXYWH(dstRect.fLeft, dstRect.fBottom,
                                              dstRect.width(), radius.fHeight);
        SkPMColor4f clearColor = MorphType::kErode == morphType
                ? SK_PMColor4fWHITE : SK_PMColor4fTRANSPARENT;
        dstRTContext->clear(clearRect, clearColor);

        srcView = dstRTContext->readSurfaceView();
        srcAlphaType = dstRTContext->colorInfo().alphaType();
        srcRect = dstRect;
    }
    if (radius.fHeight > 0) {
        auto dstRTContext = GrRenderTargetContext::Make(
                context, colorType, colorSpace, SkBackingFit::kApprox, rect.size(), 1,
                GrMipmapped::kNo, srcView.proxy()->isProtected(), kBottomLeft_GrSurfaceOrigin);
        if (!dstRTContext) {
            return nullptr;
        }

        apply_morphology_pass(dstRTContext.get(), std::move(srcView), srcAlphaType,
                              srcRect, dstRect, radius.fHeight, morphType, MorphDirection::kY);

        srcView = dstRTContext->readSurfaceView();
    }

    return SkSpecialImage::MakeDeferredFromGpu(context,
                                               SkIRect::MakeWH(rect.width(), rect.height()),
                                               kNeedNewImageUniqueID_SpecialImage,
                                               std::move(srcView), colorType,
                                               std::move(colorSpace), &input->props());
}
#endif

namespace {

#if SK_CPU_SSE_LEVEL >= SK_CPU_SSE_LEVEL_SSE2
    template<MorphType type, MorphDirection direction>
    static void morph(const SkPMColor* src, SkPMColor* dst,
                      int radius, int width, int height, int srcStride, int dstStride) {
        const int srcStrideX = direction == MorphDirection::kX ? 1 : srcStride;
        const int dstStrideX = direction == MorphDirection::kX ? 1 : dstStride;
        const int srcStrideY = direction == MorphDirection::kX ? srcStride : 1;
        const int dstStrideY = direction == MorphDirection::kX ? dstStride : 1;
        radius = std::min(radius, width - 1);
        const SkPMColor* upperSrc = src + radius * srcStrideX;
        for (int x = 0; x < width; ++x) {
            const SkPMColor* lp = src;
            const SkPMColor* up = upperSrc;
            SkPMColor* dptr = dst;
            for (int y = 0; y < height; ++y) {
                __m128i extreme = (type == MorphType::kDilate) ? _mm_setzero_si128()
                                                               : _mm_set1_epi32(0xFFFFFFFF);
                for (const SkPMColor* p = lp; p <= up; p += srcStrideX) {
                    __m128i src_pixel = _mm_cvtsi32_si128(*p);
                    extreme = (type == MorphType::kDilate) ? _mm_max_epu8(src_pixel, extreme)
                                                           : _mm_min_epu8(src_pixel, extreme);
                }
                *dptr = _mm_cvtsi128_si32(extreme);
                dptr += dstStrideY;
                lp += srcStrideY;
                up += srcStrideY;
            }
            if (x >= radius) { src += srcStrideX; }
            if (x + radius < width - 1) { upperSrc += srcStrideX; }
            dst += dstStrideX;
        }
    }

#elif defined(SK_ARM_HAS_NEON)
    template<MorphType type, MorphDirection direction>
    static void morph(const SkPMColor* src, SkPMColor* dst,
                      int radius, int width, int height, int srcStride, int dstStride) {
        const int srcStrideX = direction == MorphDirection::kX ? 1 : srcStride;
        const int dstStrideX = direction == MorphDirection::kX ? 1 : dstStride;
        const int srcStrideY = direction == MorphDirection::kX ? srcStride : 1;
        const int dstStrideY = direction == MorphDirection::kX ? dstStride : 1;
        radius = std::min(radius, width - 1);
        const SkPMColor* upperSrc = src + radius * srcStrideX;
        for (int x = 0; x < width; ++x) {
            const SkPMColor* lp = src;
            const SkPMColor* up = upperSrc;
            SkPMColor* dptr = dst;
            for (int y = 0; y < height; ++y) {
                uint8x8_t extreme = vdup_n_u8(type == MorphType::kDilate ? 0 : 255);
                for (const SkPMColor* p = lp; p <= up; p += srcStrideX) {
                    uint8x8_t src_pixel = vreinterpret_u8_u32(vdup_n_u32(*p));
                    extreme = (type == MorphType::kDilate) ? vmax_u8(src_pixel, extreme)
                                                           : vmin_u8(src_pixel, extreme);
                }
                *dptr = vget_lane_u32(vreinterpret_u32_u8(extreme), 0);
                dptr += dstStrideY;
                lp += srcStrideY;
                up += srcStrideY;
            }
            if (x >= radius) src += srcStrideX;
            if (x + radius < width - 1) upperSrc += srcStrideX;
            dst += dstStrideX;
        }
    }

#else
    template<MorphType type, MorphDirection direction>
    static void morph(const SkPMColor* src, SkPMColor* dst,
                      int radius, int width, int height, int srcStride, int dstStride) {
        const int srcStrideX = direction == MorphDirection::kX ? 1 : srcStride;
        const int dstStrideX = direction == MorphDirection::kX ? 1 : dstStride;
        const int srcStrideY = direction == MorphDirection::kX ? srcStride : 1;
        const int dstStrideY = direction == MorphDirection::kX ? dstStride : 1;
        radius = std::min(radius, width - 1);
        const SkPMColor* upperSrc = src + radius * srcStrideX;
        for (int x = 0; x < width; ++x) {
            const SkPMColor* lp = src;
            const SkPMColor* up = upperSrc;
            SkPMColor* dptr = dst;
            for (int y = 0; y < height; ++y) {
                // If we're maxing (dilate), start from 0; if minning (erode), start from 255.
                const int start = (type == MorphType::kDilate) ? 0 : 255;
                int B = start, G = start, R = start, A = start;
                for (const SkPMColor* p = lp; p <= up; p += srcStrideX) {
                    int b = SkGetPackedB32(*p),
                        g = SkGetPackedG32(*p),
                        r = SkGetPackedR32(*p),
                        a = SkGetPackedA32(*p);
                    if (type == MorphType::kDilate) {
                        B = std::max(b, B);
                        G = std::max(g, G);
                        R = std::max(r, R);
                        A = std::max(a, A);
                    } else {
                        B = std::min(b, B);
                        G = std::min(g, G);
                        R = std::min(r, R);
                        A = std::min(a, A);
                    }
                }
                *dptr = SkPackARGB32(A, R, G, B);
                dptr += dstStrideY;
                lp += srcStrideY;
                up += srcStrideY;
            }
            if (x >= radius) { src += srcStrideX; }
            if (x + radius < width - 1) { upperSrc += srcStrideX; }
            dst += dstStrideX;
        }
    }
#endif
}  // namespace

sk_sp<SkSpecialImage> SkMorphologyImageFilterImpl::onFilterImage(const Context& ctx,
                                                                 SkIPoint* offset) const {
    SkIPoint inputOffset = SkIPoint::Make(0, 0);
    sk_sp<SkSpecialImage> input(this->filterInput(0, ctx, &inputOffset));
    if (!input) {
        return nullptr;
    }

    SkIRect bounds;
    input = this->applyCropRectAndPad(this->mapContext(ctx), input.get(), &inputOffset, &bounds);
    if (!input) {
        return nullptr;
    }

    SkSize radius = mappedRadius(ctx.ctm());
    int width = SkScalarRoundToInt(radius.width());
    int height = SkScalarRoundToInt(radius.height());

    // Width (or height) must fit in a signed 32-bit int to avoid UBSAN issues (crbug.com/1018190)
    // Further, we limit the radius to something much smaller, to avoid extremely slow draw calls:
    // (crbug.com/1123035):
    constexpr int kMaxRadius = 100; // (std::numeric_limits<int>::max() - 1) / 2;

    if (width < 0 || height < 0 || width > kMaxRadius || height > kMaxRadius) {
        return nullptr;
    }

    SkIRect srcBounds = bounds;
    srcBounds.offset(-inputOffset);

    if (0 == width && 0 == height) {
        offset->fX = bounds.left();
        offset->fY = bounds.top();
        return input->makeSubset(srcBounds);
    }

#if SK_SUPPORT_GPU
    if (ctx.gpuBacked()) {
        auto context = ctx.getContext();

        // Ensure the input is in the destination color space. Typically applyCropRect will have
        // called pad_image to account for our dilation of bounds, so the result will already be
        // moved to the destination color space. If a filter DAG avoids that, then we use this
        // fall-back, which saves us from having to do the xform during the filter itself.
        input = ImageToColorSpace(input.get(), ctx.colorType(), ctx.colorSpace());

        sk_sp<SkSpecialImage> result(apply_morphology(context, input.get(), srcBounds, fType,
                                                      SkISize::Make(width, height), ctx));
        if (result) {
            offset->fX = bounds.left();
            offset->fY = bounds.top();
        }
        return result;
    }
#endif

    SkBitmap inputBM;

    if (!input->getROPixels(&inputBM)) {
        return nullptr;
    }

    if (inputBM.colorType() != kN32_SkColorType) {
        return nullptr;
    }

    SkImageInfo info = SkImageInfo::Make(bounds.size(), inputBM.colorType(), inputBM.alphaType());

    SkBitmap dst;
    if (!dst.tryAllocPixels(info)) {
        return nullptr;
    }

    SkMorphologyImageFilterImpl::Proc procX, procY;

    if (MorphType::kDilate == fType) {
        procX = &morph<MorphType::kDilate, MorphDirection::kX>;
        procY = &morph<MorphType::kDilate, MorphDirection::kY>;
    } else {
        procX = &morph<MorphType::kErode,  MorphDirection::kX>;
        procY = &morph<MorphType::kErode,  MorphDirection::kY>;
    }

    if (width > 0 && height > 0) {
        SkBitmap tmp;
        if (!tmp.tryAllocPixels(info)) {
            return nullptr;
        }

        call_proc_X(procX, inputBM, &tmp, width, srcBounds);
        SkIRect tmpBounds = SkIRect::MakeWH(srcBounds.width(), srcBounds.height());
        call_proc_Y(procY,
                    tmp.getAddr32(tmpBounds.left(), tmpBounds.top()), tmp.rowBytesAsPixels(),
                    &dst, height, tmpBounds);
    } else if (width > 0) {
        call_proc_X(procX, inputBM, &dst, width, srcBounds);
    } else if (height > 0) {
        call_proc_Y(procY,
                    inputBM.getAddr32(srcBounds.left(), srcBounds.top()),
                    inputBM.rowBytesAsPixels(),
                    &dst, height, srcBounds);
    }
    offset->fX = bounds.left();
    offset->fY = bounds.top();

    return SkSpecialImage::MakeFromRaster(SkIRect::MakeWH(bounds.width(), bounds.height()),
                                          dst, ctx.surfaceProps());
}
