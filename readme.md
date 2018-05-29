# Polymer

[![License is BSD 3-Clause](http://img.shields.io/badge/license-BSD3-blue.svg?style=flat)](https://opensource.org/licenses/BSD-3-Clause)

Platform | Build Status |
-------- | ------------ |
MSVC 2017 x64 | [![Build status]()](https://ci.appveyor.com/project/ddiakopoulos/polymer)

Polymer is a research framework for spatial interaction and real-time rendering, presently targeting C++14 and desktop-class OpenGL on Windows. It was built to explore AR/VR prototyping tools as a core concept in-engine. It is inspired by projects such as NVIDIA's [Falcor](https://github.com/NVIDIAGameWorks/Falcor), Google's [Lullaby](https://github.com/google/lullaby), and Microsoft's [Mixed Reality Toolkit](https://github.com/Microsoft/MixedRealityToolkit-Unity).  


## Features

* Physically-based gamma-correct forward renderer with MSAA
* Data-driven entity-component architecture with custom RTTI
* Lightweight, object-oriented wrapper for modern OpenGL
* Hot-reloadable assets including GLSL shaders
* Asset import for common geometry and texture formats
* Desktop scene editor application with JSON serialization
* OpenVR integration

## Architecture

Polymer is built as a collection of static libraries. `lib-polymer` is a base library consisting of common data structures and algorithms familiar to game developers. `lib-engine` depends on `lib-polymer` and introduces an entity-component system alongside a physically-based rendering pipeline. `lib-engine` also offers a runtime asset management solution. Lastly, `lib-model-io` contains code to import, export, and optimize common geometry formats (presently obj, ply, and fbx). In the future, rendering code will move to `lib-graphics` which will introduce an abstract render-hardware-interface capable of targeting multiple graphics backends. 

## Building

This repository hosts maintained project files for Visual Studio 2017. All dependencies are included in source form without the use of submodules. 

## Samples + Documentation

The API of Polymer is consistently in flux. More than 10 sample projects have been assembled in the `samples/` directory to demonstrate a variety of Polymer's features. Most projects depend on both `lib-polymer` and `lib-engine` although many do not use most features in `lib-engine` beyond GLFW window creation and event handling. A growing number of tests projects verify the correctness of some critical internal systems, but also act as an interim reference for features that lack formal documentation. 

## Contributing 

todo 

## License

Polymer is released under the BSD 3-clause license. As a framework/engine, Polymer incorporates ideas, code, and third-party libraries from a wide variety of sources. Licenses and attributions are fully documented in the `COPYING` file.

## Citing

If you use Polymer in a project that leads to publication or a public demo, we appreciate a citation. The BibTex entry is: 

```
@Misc{Diakopoulos18,
  author = {Dimitri Diakopoulos],
  title = {Polymer: An Immersive Media Prototyping Framework},
  year = {2018},
  month = {05},
  url = {https://github.com/ddiakopoulos/polymer/},
  note = {\url{https://github.com/ddiakopoulos/polymer/}}
}
```
