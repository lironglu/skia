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

Skia depends on ffmpeg to render video. You can use Homebrew to install [ffmpeg](https://ffmpeg.org/documentation.html).

```
brew install ffmpeg
```

Supporting playing video in Lottie animation is still an experimental feature. So you need to config `experimental/ffmpeg/BUILD.gn` to be able to let compiler and linker know where to find header files and static libraries needed. So I changed build file and added one argument `skia_ffmpeg_path` in [skia.gni](./gn/skia.gni) and modified [experimental/ffmpeg/BUILD.gn](./experimental/ffmpeg/BUILD.gn) to use this setting. The default value of argument `skia_ffmpeg_path` is `getenv("FFMPEG_HOME")`. So you also need to add one system environment variable `FFMPEG_HOME` to specifiy ffmepg installation folder such as `/usr/local/Cellar/ffmpeg/4.4`.

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

[skottiekit](./experimental/skottiekit/) is webassembly version of [skottie](./modules/skottie/). Be sure to set the EMSDK environment variable to the location of [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html) before building skottiekit. You can read [Emscripten Compiler Frontend (emcc)](https://emscripten.org/docs/tools_reference/emcc.html) to get details on emcc command line arguments.

### Build ffmpeg using Emscripten

In order to use ffmpeg in webassembly, ffmpeg needs to be built by Emscripten. It means ffmpeg static library files installed by Homebrew can't be used by webassembly. Please read [this article](https://jeromewu.github.io/build-ffmpeg-webassembly-version-part-2-compile-with-emscripten/) to get details on hwo to build ffmepg vebassembly version.

Please go to the parent folder of skia source tree and run the following command to clone ffmpeg 4.4 source code.

```
git clone --depth 1 --branch release/4.4 https://github.com/FFmpeg/FFmpeg.git
```

If you want to use differnt folder, do remember modifying value of variable `FFMPEG_PATH` in file [experimental/skottiekit/compile.sh) to the correct folder.

### Build skottiekit

[skottiekit](./experimental/skottiekit/) is based on Makefile to build webassembly. All of build targets are defines in this [Makefile](./experimental/skottiekit/Makefile). 

For example, run the following command to create release version of skottiekit in folder `out/skottiekit`.

```
cd experimental/skottiekit/
make release
```

Run the following command to start one Python http server to try skottiekit webassembly.

```
make serve
```

Open `http://localhost:8000` to view try skottiekit webassembly in browser. Be sure to use browser supporting ES8 because `async` and `await` are used in file [experimental/skottiekit/examples/index.html](./experimental/skottiekit/examples/index.html).

It is pitty that we can't set breakpoints in C++ source file if using source map created by emcc. So we use DWARF to debug webassembly. Please read [Debugging WebAssembly with modern tools](https://developer.chrome.com/blog/wasm-debugging-2020/) to get more details.

## View Lottie animation through Skia

Skia provide [viewer tool](./tools/viewer/) to view Lottie file. Assuming you already build `viewer` target, you can run the following command to open one Lottie file.

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