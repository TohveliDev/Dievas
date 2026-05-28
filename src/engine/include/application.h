#pragma once

// Engine
#include <layer.h>

// C++
#include <string>
#include <vector>
#include <memory>
#include <functional>

// ImGui
#include <imgui.h>
#include <imgui_internal.h>

// Vulkan
#include <vulkan/vulkan.h>

void check_vk_result(VkResult err);

struct GLFWwindow;

// Defines initial configuration parameters for the application window and runtime
struct AppSpecification
{
    std::string name = "Dievas";
    uint32_t width = 1600;
    uint32_t height = 900;
};

class Application
{
public:
    Application(const AppSpecification& appSpecification = AppSpecification());
    ~Application();

    static Application& get();

    void run();
    void setMenubarCallback(const std::function<void()>& menubarCallback) { m_menubarCallback = menubarCallback; }

    // Constructs and pushes a new layer onto the layer stack and immediately attaches it
    template<typename T>
    void pushLayer()
    {
        // Ensure T derives from Layer to enforce correct usage at compile time
        static_assert(std::is_base_of<Layer, T>::value, "Pushed type is not subclass of Layer!");

        // Create layer instance, store it, then call its attach callback
        m_layerStack.emplace_back(std::make_shared<T>())->onAttach();
    }

    void pushLayer(const std::shared_ptr<Layer>& layer) { m_layerStack.emplace_back(layer); layer->onAttach(); }

    void close() { m_isRunning = false; }

    float getTime();
    GLFWwindow* getWindowHandle() const { return m_windowHandle; }

    static VkInstance getInstance();
    static VkPhysicalDevice getPhysicalDevice();
    static VkDevice getDevice();

    static VkCommandBuffer getCommandBuffer(bool begin);
    static void flushCommandBuffer(VkCommandBuffer commandBuffer);

    static void submitResourceFree(std::function<void()>&& func);

    auto getStack() { return m_layerStack; }

private:
    void init();
    void shutdown();

private:
    AppSpecification m_specification;
    GLFWwindow* m_windowHandle = nullptr;
    bool m_isRunning = false;

    float m_timeStep = 0.0f;
    float m_frameTime = 0.0f;
    float m_lastFrameTime = 0.0f;

    float m_defaultBottom = 0.03f;
    float m_defaultSide = 0.25f;

    ImGuiID m_dockBottom;
    ImGuiID m_dockSide;

    std::vector<std::shared_ptr<Layer>> m_layerStack;
    std::function<void()> m_menubarCallback;
};

Application* createApplication(int argc, char** argv);
