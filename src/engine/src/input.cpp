// Engine
#include <input.h>
#include <application.h>

// GLFW
#include <GLFW/glfw3.h>

// Returns true if a key is currently pressed or being held down
bool Input::isKeyDown(KeyCode keycode)
{
	GLFWwindow* windowHandle = Application::get().getWindowHandle();
	int state = glfwGetKey(windowHandle, (int)keycode);
	return state == GLFW_PRESS || state == GLFW_REPEAT;
}

// Returns true if a mouse button is currently pressed
bool Input::isMouseButtonDown(MouseButton button)
{
	GLFWwindow* windowHandle = Application::get().getWindowHandle();
	int state = glfwGetMouseButton(windowHandle, (int)button);
	return state == GLFW_PRESS;
}

// Returns current mouse cursor position in window coordinates
glm::vec2 Input::getMousePosition()
{
	GLFWwindow* windowHandle = Application::get().getWindowHandle();
	
	double x, y;
	glfwGetCursorPos(windowHandle, &x, &y);
	return { (float)x, (float)y };
}

// Sets cursor mode (normal, hidden, locked)
void Input::setCursorMode(CursorMode mode)
{
	GLFWwindow* windowHandle = Application::get().getWindowHandle();
	glfwSetInputMode(windowHandle, GLFW_CURSOR, GLFW_CURSOR_NORMAL + (int)mode);
}
