# Dievas
Dievas is a 3D Software Renderer created with Vulkan. The goal of Dievas is to provide a simple to use 3D editor, that does not require any specialized hardware for raytracing, as the raytracing logic is calculated and rendered on the CPU. Dievas is built to be the perfect balance between performance and quality.

## Features
- Intuitive UI
- Versatile Material editor
- Fully customizable Skybox
- Realistic lighting (Pathtracing)
- 3D Model support (.obj files)
- Built-in Screenshotting

## Requirements
To compile and use Dievas, you need the [Vulkan SDK](https://www.lunarg.com/products/vulkan-sdk/) and [CMake](https://cmake.org/). All other required Third Party Libraries are provided in the repository. The CMake project will automatically locate Vulkan SDK on your machine, so the only thing you need to build Dievas is to [build](https://cmake.org/cmake/help/latest/guide/tutorial/Before%20You%20Begin.html#try-it-out) the project with CMake.
