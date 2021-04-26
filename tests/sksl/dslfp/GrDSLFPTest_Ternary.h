

/**************************************************************************************************
 *** This file was autogenerated from GrDSLFPTest_Ternary.fp; do not modify.
 **************************************************************************************************/
#ifndef GrDSLFPTest_Ternary_DEFINED
#define GrDSLFPTest_Ternary_DEFINED

#include "include/core/SkM44.h"
#include "include/core/SkTypes.h"


#include "src/gpu/GrFragmentProcessor.h"

class GrDSLFPTest_Ternary : public GrFragmentProcessor {
public:
    static std::unique_ptr<GrFragmentProcessor> Make(SkPMColor4f colorGreen, SkPMColor4f colorRed) {
        return std::unique_ptr<GrFragmentProcessor>(new GrDSLFPTest_Ternary(colorGreen, colorRed));
    }
    GrDSLFPTest_Ternary(const GrDSLFPTest_Ternary& src);
    std::unique_ptr<GrFragmentProcessor> clone() const override;
    const char* name() const override { return "DSLFPTest_Ternary"; }
    SkPMColor4f colorGreen;
    SkPMColor4f colorRed;
private:
    GrDSLFPTest_Ternary(SkPMColor4f colorGreen, SkPMColor4f colorRed)
    : INHERITED(kGrDSLFPTest_Ternary_ClassID, kNone_OptimizationFlags)
    , colorGreen(colorGreen)
    , colorRed(colorRed) {
    }
    std::unique_ptr<GrGLSLFragmentProcessor> onMakeProgramImpl() const override;
    void onGetGLSLProcessorKey(const GrShaderCaps&, GrProcessorKeyBuilder*) const override;
    bool onIsEqual(const GrFragmentProcessor&) const override;
#if GR_TEST_UTILS
    SkString onDumpInfo() const override;
#endif
    GR_DECLARE_FRAGMENT_PROCESSOR_TEST
    using INHERITED = GrFragmentProcessor;
};
#endif