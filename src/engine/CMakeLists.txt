set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Vulkan REQUIRED)

# Engine files
file(GLOB_RECURSE ENGINE_INC_FILES "include/*.h")
file(GLOB_RECURSE ENGINE_SRC_FILES "src/*.cpp")

# ImGui Files
set(IMGUI_DIR "${PROJECT_SOURCE_DIR}/ext/imgui")

file(GLOB IMGUI_SRC "${IMGUI_DIR}/*.cpp")

set(IMGUI_BACKEND_SRC
	"${IMGUI_DIR}/backends/imgui_impl_glfw.cpp"
	"${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp"
)

add_library(engine ${ENGINE_INC_FILES} ${ENGINE_SRC_FILES} ${IMGUI_SRC} ${IMGUI_BACKEND_SRC} ${FONT_FILE})

target_include_directories(engine PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/assets")

target_link_libraries(engine PUBLIC glfw glm tinyobjloader Vulkan::Vulkan)
target_include_directories(engine PUBLIC "${PROJECT_SOURCE_DIR}/ext/stb-master" "${IMGUI_DIR}" "${IMGUI_DIR}/backends")