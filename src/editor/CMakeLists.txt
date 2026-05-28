set(HEADERS
)

set(SOURCES
)

include_directories("${GLFW_SOURCE_DIR}/deps")

add_executable(editor dievas_editor.cpp)
target_link_libraries(editor engine)