# Computer graphics in Game development

This repo contains a implementations of different rendering techniques from scratch

## Requirements

- [CMake](https://cmake.org/download/)
- C++17 compatible C++ compiler
- [OpenMP library](https://www.openmp.org/)

For DirectX12 you need a Windows machine or VM with installed software and [Windows SDK](https://developer.microsoft.com/en-us/windows/downloads/windows-sdk/)

## How to build the solution

Go to the project folder and run the next command:

```sh
mkdir build
cd build
cmake ..
cmake --build . --config Release
```
The folder `build/Release/` should contain the resuling executalbes

## Third-party tools and data

- [STB](https://github.com/nothings/stb) by Sean Barrett (Public Domain)
- [Linalg.h](https://github.com/sgorsten/linalg) by Sterling Orsten (The Unlicense)
- [Tinyobjloader](https://github.com/syoyo/tinyobjloader) by Syoyo Fujita (MIT License)
- [Cxxopts](https://github.com/jarro2783/cxxopts) by jarro2783 (MIT License)
- [Cornell Box models](https://casual-effects.com/g3d/data10/index.html#) by Morgan McGuire (CC BY 3.0 License)
- [Cube model](https://casual-effects.com/g3d/data10/index.html#) by Morgan McGuire (CC BY 3.0 License)
- [Teapot model](https://casual-effects.com/g3d/data10/common/model/teapot/teapot.zip) by Martin Newell (CC0 License)
- [Dabrovic Sponza model](https://casual-effects.com/g3d/data10/index.html#) by Marko Dabrovic (CC BY-NC License)
