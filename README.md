# Image Viewer Application Challenge entry
A simple image viewer. Dependencies are [GLFW](https://glfw.org) and OpenGL 4.3+ (for `glDebugMessageCallback`).

## Building
I have only tested this on Linux, but should work on Windows with a bit of fiddling.
```console
$ cmake -B build -DCMAKE_BUILD_TYPE=Release
$ cmake --build build
```
## Running
IVAC accepts the image name as an argument. It uses [stb_image.h](https://github.com/nothings/stb) for loading images.
```console
$ ./build/ivac /path/to/my/image.jpg
```
