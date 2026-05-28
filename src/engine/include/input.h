#pragma once

// GLM
#include <glm/glm.hpp>

// Engine
#include <keycodes.h>

class Input
{
public:
    static bool isKeyDown(KeyCode keycode);
    static bool isMouseButtonDown(MouseButton button);

    static glm::vec2 getMousePosition();

    static void setCursorMode(CursorMode mode);
};