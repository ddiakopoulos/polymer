# Polymer

[![License is BSD 3-Clause](https://img.shields.io/badge/license-BSD3-blue.svg?style=flat)](https://opensource.org/licenses/BSD-3-Clause)

Platform | Build Status |
-------- | ------------ |
MSVC 2017 x64 | [![Build status](https://ci.appveyor.com/api/projects/status/3hjvf03s8bwjciva?svg=true)](https://ci.appveyor.com/project/ddiakopoulos/polymer)

<p align="center">
  <img src="https://s3-us-west-1.amazonaws.com/polymer-engine/polymer-github-hero.png"/>
</p>

Polymer is a research framework for spatial interaction and real-time rendering, presently targeting C++14 and desktop-class OpenGL on Windows. It was built to explore AR/VR prototyping tools as a core concept in-engine. It is inspired by projects such as NVIDIA's [Falcor](https://github.com/NVIDIAGameWorks/Falcor), Google's [Lullaby](https://github.com/google/lullaby), and Microsoft's [Mixed Reality Toolkit](https://github.com/Microsoft/MixedRealityToolkit-Unity). While the primary focus of Polymer is immersive media, the engine contains features useful to graphics research and general tool and utility development as well. 

## Features

* Physically-based, gamma-correct forward renderer with MSAA
* Data-driven entity-component architecture with custom RTTI
* Lightweight, object-oriented wrapper for modern OpenGL
* Hot-reloadable assets including GLSL shaders
* Asset import for common geometry and texture formats
* Desktop scene editor application with json scene serialization
* OpenVR integration

## Architecture

Polymer is designed as a collection of static libraries. `lib-polymer` is a base library consisting of common data structures and algorithms familiar to game developers. `lib-engine` depends on `lib-polymer` and introduces an entity-component system alongside a physically-based rendering pipeline. `lib-engine` also offers a runtime asset management solution. Lastly, `lib-model-io` contains code to import, export, and optimize common geometry formats (presently obj, ply, and fbx). 

In the future, rendering code will move to `lib-graphics` with an abstract render-hardware-interface capable of targeting multiple graphics backends such as Metal and Vulkan.

## Building

Polymer requires a recent version of Windows 10 alongside a graphics driver capable of supporting an OpenGL 4.5 context. This repository hosts maintained project files for Visual Studio 2017. Most dependencies are included in source form without the use of submodules. 

### Prerequisites

Windows SDK is required to build the project. You may download it [here](https://developer.microsoft.com/en-us/windows/downloads/windows-10-sdk)

Use of the FBX 2017 SDK is gated by an environment variable (`SYSTEM_HAS_FBX_SDK` which should be set to evaluate to an identical token, `SYSTEM_HAS_FBX_SDK`); the extracted FBX SDK itself must be located in `polymer\lib-model-io\third-party\fbxsdk`. You may download the FBX SDK from [here](https://www.autodesk.com/developer-network/platform-technologies/fbx-sdk-2019-0)

### Building the projects

Open the solution file in Microsoft Visual Studio 2017 and build it.

**Note:** If the build fails with the message _Windows SDK version 10.0.16299.0 not found_ and you have installed the Windows SDK correctly, you might need to change the Windows SDK version in each project properties and select the one you've installed.

## Samples & Documentation

The API of Polymer is in flux. A growing number of sample projects have been assembled in the `samples/` directory to demonstrate a variety of Polymer's libraries and features. Most projects depend on both `lib-polymer` and `lib-engine` although many do not use features in `lib-engine` beyond GLFW window creation and event handling. A small number of tests projects verify the correctness of critical internal systems, but also act as an interim reference for functions or objects lacking formal documentation. 

## Contributing 

Polymer is in early-stages of development and welcomes both experienced and inexperienced open-source contributors. The GitHub issue queue is a good place to start in understanding the scope and priority of upcoming features. For larger pull requests and feature implementations, it is good practice confirm with the project maintainer(s) before embarking on the work. 

## License

Polymer is released under the BSD 3-clause license. As a framework/engine, Polymer incorporates ideas, code, and third-party libraries from a wide variety of sources. Licenses and attributions are fully documented in the `COPYING` file.

## Citing

If you use Polymer in a project that leads to publication or a public demo, we appreciate a citation. The BibTex entry is: 

```
@Polymer{Diakopoulos19,
  author = {Dimitri Diakopoulos],
  title  = {Polymer: A Prototyping Framework for Spatial Computing},
  year   = {2019},
  month  = {01},
  url    = {https://github.com/ddiakopoulos/polymer/},
  note   = {\url{https://github.com/ddiakopoulos/polymer/}}
}
```
