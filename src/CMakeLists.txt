cmake_minimum_required(VERSION 3.15.0)
project(Dievas)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory("ext")
add_subdirectory("engine")
add_subdirectory("editor")

include_directories("${GLFW_SOURCE_DIR}/deps")
include_directories("ext/stb-master")
include_directories("ext/imgui")

