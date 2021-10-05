# SKIA

[Skia](https://skia.org) is a open-source 2D graphic library for drawing Text, Geometries, and Images. It also supports rendering [Lottie](https://github.com/airbnb/lottie), [Rive](https://rive.app/) animation。

Lottie doesn't support video element. Skia extend Lottie to support playing video in Lottie animation and exporting Lottie animation.

## Build Skia on MacOS

### Install depot_tools and Git

Follow the instructions on Installing Chromium’s depot_tools to download depot_tools (which includes gclient, git-cl, and Ninja). Below is a summary of the necessary steps.

git clone 'https://chromium.googlesource.com/chromium/tools/depot_tools.git'
export PATH="${PWD}/depot_tools:${PATH}"

depot_tools will also install Git on your system, if it wasn’t installed already.

### Clone the Skia repository

```
git clone https://skia.googlesource.com/skia.git
cd skia
python2 ./bin/fetch-gn
python2 tools/git-sync-deps
```

### Install ffmpeg

Skia depends on ffmpeg to render video. You can use Homebrew to install ffmpeg.

```
brew install ffmpeg
```

It looks that supporting playing video in Lottie animation is still an experimental feature. So you need to config `experimental/ffmpeg/BUILD.gn` to be able to let compiler and linker know where to find header files and static libraries needed. So I added one argument `skia_ffmpeg_path` in `skia.gni` and modified `experimental/ffmpeg/BUILD.gn` to use those configurations.

### Generate build files

Run `gn gen` to generate build files. As arguments to `gn gen`, pass a name for your build directory, and optionally --args= to configure the build type.

Assuming ffmpeg is install in folder `/usr/local/Cellar/ffmpeg/4.4`, you can use the following command to generate [ninja](https://ninja-build.org/) build files with ffmpeg supporting.

```
bin/gn gen out/release --args='target_cpu="x64" skia_use_ffmpeg=true skia_ffmpeg_path="/usr/local/Cellar/ffmpeg/4.4"'
```

### Build targets

You can lists all targets by running `gn ls` command.

```
bin/gn ls out/release
```

The following targets are related to lottie.
- viewer: View standard Skia or Lottie animation. You can adjust many parameters in this viewer to check their effects.
- skottie2movie: Convert Lottie animation to mp4 video.

Run the following command to build viewer

```
ninja -C out/release viewer
```

### Build Skia webassembly

[skottiekit](./experimental/skottiekit/) is webassembly version of [skottie](./modules/skottie/). Be sure to set the EMSDK environment variable to the location of [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html) before building skottiekit.

skottiekit is based on Makefile to build webassembly. All of build targets are defines in this [Makefile](./experimental/skottiekit/Makefile). 

For example, run the following command to create release version of skottiekit in folder `out/skottiekit`.

```
cd experimental/skottiekit/
make release
```

Run the following command to start one Python http server to try skottiekit webassembly.

```
make serve
```

Open `http://localhost:8000` to view try skottiekit webassembly in browser. If port `8000` is already occupied, please try port `8001`.

## View Lottie animation through Skia

Skia provide [viewer too](./tools/viewer/) to view Lottie file. Assuming you already build `viewer` target, you can run the following command to open one Lottie file.

```
out/release/viewer -f resources/skottie/skottie_sample_1.json
```

Skia also provides lots of Lottie sample files in folder `resources/skottie`. Viewer tool have many command line arguments, you can run the following command to get details.

```
out/release/viewer -h
```

Viewer GUI also provides some panels and commands to manipulate or adjust animation.
- up/down key: zoom in/zoom out window
- blank: open tool panel
- d: change rendering backend
- g: show gui main menu
- ......

You can read file [tools/viewer/Viewer.cpp](./tools/viewer/Viewer.cpp) to get more details.

## Convert Lottie animation to video

Skia provide [skottie2movie](./tools/skottie2movie.cpp) to convert Lottie animation to video. Assuming you already build `skottie2movie` target, you can run the following command to convert one Lottie file to mp4 video.

```
out/release/skottie2movie -g -i resources/skottie/skottie_sample_1.json -o skottie_sample_1.mp4
```

Run the following command to get details on this tool.

```
out/release/skottie2movie -h
```