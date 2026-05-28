// Engine
#include <application.h>

// ImGui
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>

// C++
#include <stdio.h>
#include <stdlib.h>
#include <iostream>

// GLFW
#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

// Vulkan
#include <vulkan/vulkan.h>

// GLM
#include <glm/glm.hpp>

// Font
#include <roboto_regular.h>

extern bool g_applicationRunning;

#ifdef _DEBUG
#define IMGUI_VULKAN_DEBUG_REPORT
#endif


// Initializing all Vulkan specific variables
static VkAllocationCallbacks* g_allocator = NULL;
static VkInstance				g_instance = VK_NULL_HANDLE;
static VkPhysicalDevice			g_physicalDevice = VK_NULL_HANDLE;
static VkDevice					g_device = VK_NULL_HANDLE;
static uint32_t					g_queueFamily = (uint32_t)-1;
static VkQueue					g_queue = VK_NULL_HANDLE;
static VkDebugReportCallbackEXT g_debugReport = VK_NULL_HANDLE;
static VkPipelineCache			g_pipelineCache = VK_NULL_HANDLE;
static VkDescriptorPool			g_descriptorPool = VK_NULL_HANDLE;

static ImGui_ImplVulkanH_Window g_mainWindowData;
static int						g_minImageCount = 2;
static bool						g_swapChainRebuild = false;

// Per-frame-in-flight
static std::vector<std::vector<VkCommandBuffer>>	   s_allocatedCommandBuffers;
static std::vector<std::vector<std::function<void()>>> s_resourceFreeQueue;

static uint32_t s_currentFrameIndex = 0;

static Application* s_instance = nullptr;

// Validates Vulkan API call results and aborts on fatal errors.
void check_vk_result(VkResult err)
{
	// Success case: no error to report
	if (err == 0) return;

	// Log Vulkan error code for debugging
	fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);

	// Vulkan negative values indicate fatal failures
	if (err < 0) abort();
}

// Optional Vulkan debug callback used when validation layers are enabled
#ifdef IMGUI_VULKAN_DEBUG_REPORT
static VKAPI_ATTR VkBool32 VKAPI_CALL debug_report(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t location, int32_t messageCode, const char* pLayerPrefix, const char* pMessage, void* pUserData)
{
	// Unused parameters (required by callback signature)
	(void)flags; (void)object; (void)location; (void)messageCode; (void)pUserData; (void)pLayerPrefix;

	// Print validation/debug messages from Vulkan runtime
	fprintf(stderr, "[vulkan] Debug report from ObjectType: %i\nMessage: %s\n\n", objectType, pMessage);

	// VK_FALSE indicates execution should continue
	return VK_FALSE;
}
#endif

// Initializes Vulkan instance, physical device, logical device, and descriptor pool.
static void setupVulkan(const char** extensions, uint32_t extensionsCount)
{
	VkResult err;

	// Create Vulkan instance
	{
		VkInstanceCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.enabledExtensionCount = extensionsCount;
		createInfo.ppEnabledExtensionNames = extensions;
#ifdef IMGUI_VULKAN_DEBUG_REPORT
		// Enable validation layers when debug reporting is enabled
		const char* layers[] = { "VK_LAYER_KHRONOS_validation" };
		createInfo.enabledLayerCount = 1;
		createInfo.ppEnabledLayerNames = layers;

		const char** extensionsExt = (const char**)malloc(sizeof(const char*) * (extensionsCount + 1));
		memcpy(extensionsExt, extensions, extensionsCount * sizeof(const char*));
		extensionsExt[extensionsCount] = "VK_EXT_debug_report";
		createInfo.enabledExtensionCount = extensionsCount + 1;
		createInfo.ppEnabledExtensionNames = extensionsExt;

		err = vkCreateInstance(&createInfo, g_allocator, &g_instance);
		check_vk_result(err);
		free(extensionsExt);

		auto vkCreateDebugReportCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(g_instance, "vkCreateDebugReportCallbackEXT");
		IM_ASSERT(vkCreateDebugReportCallbackEXT != NULL);

		VkDebugReportCallbackCreateInfoEXT debugReportCI = {};
		debugReportCI.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
		debugReportCI.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
		debugReportCI.pfnCallback = debug_report;
		debugReportCI.pUserData = NULL;
		err = vkCreateDebugReportCallbackEXT(g_instance, &debugReportCI, g_allocator, &g_debugReport);
		check_vk_result(err);
#else
		// Create Vulkan instance without validation layers
		err = vkCreateInstance(&createInfo, g_allocator, &g_instance);
		check_vk_result(err);
		IM_UNUSED(g_debugReport);
#endif
	}

	// Select physical device (GPU)
	{
		uint32_t gpuCount;
		err = vkEnumeratePhysicalDevices(g_instance, &gpuCount, NULL);
		check_vk_result(err);
		IM_ASSERT(gpuCount > 0);

		VkPhysicalDevice* gpus = (VkPhysicalDevice*)malloc(sizeof(VkPhysicalDevice) * gpuCount);
		err = vkEnumeratePhysicalDevices(g_instance, &gpuCount, gpus);
		check_vk_result(err);

		// Prefer discrete GPU if available
		int useGPU = 0;
		for (int i = 0; i < (int)gpuCount; i++)
		{
			VkPhysicalDeviceProperties properties;
			vkGetPhysicalDeviceProperties(gpus[i], &properties);

			if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
			{
				useGPU = i;
				break;
			}
		}

		g_physicalDevice = gpus[useGPU];
		free(gpus);
	}

	// Select graphics queue family
	{
		uint32_t count;
		vkGetPhysicalDeviceQueueFamilyProperties(g_physicalDevice, &count, NULL);
		VkQueueFamilyProperties* queues = (VkQueueFamilyProperties*)malloc(sizeof(VkQueueFamilyProperties) * count);
		vkGetPhysicalDeviceQueueFamilyProperties(g_physicalDevice, &count, queues);

		// Find a queue that supports graphics operations
		for (uint32_t i = 0; i < count; i++)
		{
			if (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				g_queueFamily = i;
				break;
			}
		}

		free(queues);
		IM_ASSERT(g_queueFamily != (uint32_t)-1);
	}

	// Create logical device and graphics queue
	{
		int deviceExtensionCount = 1;
		const char* deviceExtensions[] = { "VK_KHR_swapchain" };
		const float queuePriority[] = { 1.0f };
		VkDeviceQueueCreateInfo queueInfo[1] = {};
		queueInfo[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueInfo[0].queueFamilyIndex = g_queueFamily;
		queueInfo[0].queueCount = 1;
		queueInfo[0].pQueuePriorities = queuePriority;

		VkDeviceCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		createInfo.queueCreateInfoCount = sizeof(queueInfo) / sizeof(queueInfo[0]);
		createInfo.pQueueCreateInfos = queueInfo;
		createInfo.enabledExtensionCount = deviceExtensionCount;
		createInfo.ppEnabledExtensionNames = deviceExtensions;
		err = vkCreateDevice(g_physicalDevice, &createInfo, g_allocator, &g_device);
		check_vk_result(err);
		vkGetDeviceQueue(g_device, g_queueFamily, 0, &g_queue);
	}

	// Create descriptor pool for ImGui/Vulkan resource allocation
	{
		VkDescriptorPoolSize poolSizes[] =
		{
			{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
			{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
			{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
		};

		VkDescriptorPoolCreateInfo poolInfo = {};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
		poolInfo.maxSets = 1000 * IM_ARRAYSIZE(poolSizes);
		poolInfo.poolSizeCount = (uint32_t)IM_ARRAYSIZE(poolSizes);
		poolInfo.pPoolSizes = poolSizes;
		err = vkCreateDescriptorPool(g_device, &poolInfo, g_allocator, &g_descriptorPool);
	}
}

// Configures a Vulkan rendering surface and creates swapchain resources for an ImGui window
static void setupVulkanWindow(ImGui_ImplVulkanH_Window* wd, VkSurfaceKHR surface, int width, int height)
{
	// Attach platform surface to ImGui Vulkan window wrapper
	wd->Surface = surface;

	VkBool32 res;

	// Verify that the selected queue family supports presentation to this surface
	vkGetPhysicalDeviceSurfaceSupportKHR(g_physicalDevice, g_queueFamily, wd->Surface, &res);

	// Fail early if the device cannot present to the window system
	if (res != VK_TRUE)
	{
		fprintf(stderr, "Error no WSI support on Physical Device 0\n");
		exit(-1);
	}

	// Preferred surface image formats (in order of preference)
	const VkFormat requestSurfaceImageFormat[] = { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM };
	const VkColorSpaceKHR requestSurfaceColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;

	// Choose the best supported surface format
	wd->SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(g_physicalDevice, wd->Surface, requestSurfaceImageFormat, (size_t)IM_ARRAYSIZE(requestSurfaceImageFormat), requestSurfaceColorSpace);

#ifdef IMGUI_UNLIMITED_FRAME_RATE
	// Prefer low-latency present modes when uncapped FPS is allowed
	VkPresentModeKHR presentModes[] = { VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR, VK:PRESENT_MODE_FIFO_KHR };
#else
	// Fallback to guaranteed present mode (vsync)
	VkPresentModeKHR presentModes[] = { VK_PRESENT_MODE_FIFO_KHR };
#endif
	// Select best supported present mode
	wd->PresentMode = ImGui_ImplVulkanH_SelectPresentMode(g_physicalDevice, wd->Surface, &presentModes[0], IM_ARRAYSIZE(presentModes));

	// Swapchain must support at least double buffering
	IM_ASSERT(g_minImageCount >= 2);

	// Create or resize swapchain and associated framebuffers/resources
	ImGui_ImplVulkanH_CreateOrResizeWindow(g_instance, g_physicalDevice, g_device, wd, g_queueFamily, g_allocator, width, height, g_minImageCount);
}

// Releases all Vulkan resources created during application lifetime
static void cleanupVulkan()
{
	// Destroy global descriptor pool used by ImGui and GPU resources
	vkDestroyDescriptorPool(g_device, g_descriptorPool, g_allocator);

#ifdef IMGUI_VULKAN_DEBUG_REPORT
	// Destroy debug callback if validation layers were enabled
	auto vkDestroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(g_instance, "vkDestroyDebugReportCallbackEXT");
	vkDestroyDebugReportCallbackEXT(g_instance, g_debugReport, g_allocator);
#endif

	// Destroy logical device before instance
	vkDestroyDevice(g_device, g_allocator);

	// Destroy Vulkan instance as final step
	vkDestroyInstance(g_instance, g_allocator);
}

// Cleans up swapchain and window-specific Vulkan resources
static void cleanupVulkanWindow()
{
	// Destroy swapchain, framebuffers, command buffers, and image views
	ImGui_ImplVulkanH_DestroyWindow(g_instance, g_device, &g_mainWindowData, g_allocator);
}

// Renders a single frame using Vulkan swapchain synchronization and ImGui draw data
static void renderFrame(ImGui_ImplVulkanH_Window* wd, ImDrawData* drawData)
{
	VkResult err;

	// Semaphores used for GPU synchronization between acquire and render stages
	VkSemaphore imgAcquiredSemaphore = wd->FrameSemaphores[wd->SemaphoreIndex].ImageAcquiredSemaphore;
	VkSemaphore renderCompleteSemaphore = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;

	// Acquire next swapchain image for rendering
	err = vkAcquireNextImageKHR(g_device, wd->Swapchain, UINT64_MAX, imgAcquiredSemaphore, VK_NULL_HANDLE, &wd->FrameIndex);

	// Handle swapchain resize or invalidation
	if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
	{
		g_swapChainRebuild = true;
		return;
	}

	check_vk_result(err);

	// Advance per-frame index for resource tracking
	s_currentFrameIndex = (s_currentFrameIndex + 1) % g_mainWindowData.ImageCount;

	ImGui_ImplVulkanH_Frame* fd = &wd->Frames[wd->FrameIndex];

	// Ensure previous GPU work on this frame is complete
	{
		err = vkWaitForFences(g_device, 1, &fd->Fence, VK_TRUE, UINT64_MAX);
		check_vk_result(err);

		err = vkResetFences(g_device, 1, &fd->Fence);
		check_vk_result(err);
	}

	// Execute deferred CPU-side resource cleanup tied to this frame index
	{
		for (auto& func : s_resourceFreeQueue[s_currentFrameIndex])
			func();
		s_resourceFreeQueue[s_currentFrameIndex].clear();
	}

	// Reset and recreate command buffer for this frame
	{
		auto& allocatedCmdBuffers = s_allocatedCommandBuffers[wd->FrameIndex];

		// Free any previously allocated secondary command buffers
		if (allocatedCmdBuffers.size() > 0)
		{
			vkFreeCommandBuffers(g_device, fd->CommandPool, (uint32_t)allocatedCmdBuffers.size(), allocatedCmdBuffers.data());
			allocatedCmdBuffers.clear();
		}

		// Reset command pool for fresh recording
		err = vkResetCommandPool(g_device, fd->CommandPool, 0);
		check_vk_result(err);

		// Begin recording primary command buffer
		VkCommandBufferBeginInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		err = vkBeginCommandBuffer(fd->CommandBuffer, &info);
		check_vk_result(err);
	}

	// Begin render pass targeting current swapchain framebuffer
	{
		VkRenderPassBeginInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		info.renderPass = wd->RenderPass;
		info.framebuffer = fd->Framebuffer;
		info.renderArea.extent.width = wd->Width;
		info.renderArea.extent.height = wd->Height;
		info.clearValueCount = 1;
		info.pClearValues = &wd->ClearValue;
		vkCmdBeginRenderPass(fd->CommandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
	}

	// Record ImGui draw commands into the command buffer
	ImGui_ImplVulkan_RenderDrawData(drawData, fd->CommandBuffer);

	vkCmdEndRenderPass(fd->CommandBuffer);

	// Submit command buffer to graphics queue
	{
		VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		VkSubmitInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		info.waitSemaphoreCount = 1;
		info.pWaitSemaphores = &imgAcquiredSemaphore;
		info.pWaitDstStageMask = &waitStage;
		info.commandBufferCount = 1;
		info.pCommandBuffers = &fd->CommandBuffer;
		info.signalSemaphoreCount = 1;
		info.pSignalSemaphores = &renderCompleteSemaphore;

		err = vkEndCommandBuffer(fd->CommandBuffer);
		check_vk_result(err);
		err = vkQueueSubmit(g_queue, 1, &info, fd->Fence);
		check_vk_result(err);
	}
}

// Presents the rendered swapchain image to the screen.
static void framePresent(ImGui_ImplVulkanH_Window* wd)
{
	// Skip presentation if swapchain is being rebuilt
	if (g_swapChainRebuild) return;

	// Semaphore that signals rendering completion for this frame
	VkSemaphore renderCompleteSemaphore = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;

	// Present the rendered image to the swapchain
	VkPresentInfoKHR info = {};
	info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	info.waitSemaphoreCount = 1;
	info.pWaitSemaphores = &renderCompleteSemaphore;
	info.swapchainCount = 1;
	info.pSwapchains = &wd->Swapchain;
	info.pImageIndices = &wd->FrameIndex;
	VkResult err = vkQueuePresentKHR(g_queue, &info);

	// Handle swapchain invalidation (resize, format change, etc.)
	if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
	{
		g_swapChainRebuild = true;
		return;
	}

	check_vk_result(err);

	// Advance semaphore index for next frame in flight
	wd->SemaphoreIndex = (wd->SemaphoreIndex + 1) % wd->ImageCount;
}

// GLFW error callback for logging windowing system errors
static void glfw_error_callback(int error, const char* description)
{
	fprintf(stderr, "GLFW error %d: %s\n", error, description);
}

// Constructs the application, initializes Vulkan and windowing system
Application::Application(const AppSpecification& appSpecification)
	: m_specification(appSpecification)
{
	s_instance = this;

	init();
}

// Destroys application and releases all runtime resources
Application::~Application()
{
	shutdown();

	s_instance = nullptr;
}

// Returns singleton instance of the application
Application& Application::get()
{
	return *s_instance;
}

// Initializes GLFW, Vulkan, ImGui, swapchain, and rendering context
void Application::init()
{
	glfwSetErrorCallback(glfw_error_callback);

	if (!glfwInit())
	{
		std::cerr << "Could not initialize GLFW!\n";
		return;
	}

	// Disable OpenGL context creation (we use Vulkan)
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	// Start window maximized
	glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);

	// Create GLFW window
	m_windowHandle = glfwCreateWindow(m_specification.width, m_specification.height, m_specification.name.c_str(), NULL, NULL);

	// Ensure Vulkan is supported by GLFW backend
	if (!glfwVulkanSupported())
	{
		std::cerr << "GLFW: Vulkan not supported!\n";
		return;
	}

	// Create Vulkan instance using required GLFW extensions
	uint32_t extensionCount = 0;
	const char** extensions = glfwGetRequiredInstanceExtensions(&extensionCount);
	setupVulkan(extensions, extensionCount);

	// Create Vulkan surface for the GLFW window
	VkSurfaceKHR surface;
	VkResult err = glfwCreateWindowSurface(g_instance, m_windowHandle, g_allocator, &surface);
	check_vk_result(err);

	// Setup swapchain and per-frame Vulkan resources
	int w, h;
	glfwGetFramebufferSize(m_windowHandle, &w, &h);
	ImGui_ImplVulkanH_Window* wd = &g_mainWindowData;
	setupVulkanWindow(wd, surface, w, h);

	// Allocate per-frame command tracking structures
	s_allocatedCommandBuffers.resize(wd->ImageCount);
	s_resourceFreeQueue.resize(wd->ImageCount);

	// Initialize ImGui core context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();


	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

	ImGui::StyleColorsDark();

	// Adjust style for multi-viewport rendering
	ImGuiStyle& style = ImGui::GetStyle();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}

	// Initialize GLFW + Vulkan ImGui backend
	ImGui_ImplGlfw_InitForVulkan(m_windowHandle, true);
	ImGui_ImplVulkan_InitInfo initInfo = {};
	initInfo.Instance = g_instance;
	initInfo.PhysicalDevice = g_physicalDevice;
	initInfo.Device = g_device;
	initInfo.QueueFamily = g_queueFamily;
	initInfo.Queue = g_queue;
	initInfo.PipelineCache = g_pipelineCache;
	initInfo.DescriptorPool = g_descriptorPool;
	initInfo.Subpass = 0;
	initInfo.MinImageCount = g_minImageCount;
	initInfo.ImageCount = wd->ImageCount;
	initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
	initInfo.Allocator = g_allocator;
	initInfo.CheckVkResultFn = check_vk_result;
	ImGui_ImplVulkan_Init(&initInfo, wd->RenderPass);

	// Load application font into ImGui
	ImFontConfig fontConfig;
	fontConfig.FontDataOwnedByAtlas = false;
	ImFont* robotoFont = io.Fonts->AddFontFromMemoryTTF((void*)g_RobotoRegular, sizeof(g_RobotoRegular), 20.0f, &fontConfig);
	io.FontDefault = robotoFont;

	// Upload ImGui font texture to GPU using a temporary command buffer
	{
		VkCommandPool cmdPool = wd->Frames[wd->FrameIndex].CommandPool;
		VkCommandBuffer cmdBuffer = wd->Frames[wd->FrameIndex].CommandBuffer;

		err = vkResetCommandPool(g_device, cmdPool, 0);
		check_vk_result(err);
		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		err = vkBeginCommandBuffer(cmdBuffer, &beginInfo);
		check_vk_result(err);

		ImGui_ImplVulkan_CreateFontsTexture(cmdBuffer);

		VkSubmitInfo endInfo = {};
		endInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		endInfo.commandBufferCount = 1;
		endInfo.pCommandBuffers = &cmdBuffer;
		err = vkEndCommandBuffer(cmdBuffer);
		check_vk_result(err);
		err = vkQueueSubmit(g_queue, 1, &endInfo, VK_NULL_HANDLE);
		check_vk_result(err);

		// Wait until GPU finishes uploading font texture
		err = vkDeviceWaitIdle(g_device);
		check_vk_result(err);
		ImGui_ImplVulkan_DestroyFontUploadObjects();
	}
}

// Shuts down the application and releases all GPU, UI, and system resources
void Application::shutdown()
{
	// Notify all layers that they are being detached (allow cleanup)
	for (auto& layer : m_layerStack)
		layer->onDetach();

	// Clear layer stack before destroying rendering resources
	m_layerStack.clear();

	// Ensure GPU has finished all pending work before destruction
	VkResult err = vkDeviceWaitIdle(g_device);
	check_vk_result(err);

	// Execute any deferred per-frame resource destruction callbacks
	for (auto& queue : s_resourceFreeQueue)
	{
		for (auto& func : queue)
			func();
	}

	// Clear cleanup queues after execution
	s_resourceFreeQueue.clear();

	// Shutdown ImGui backends and destroy UI context
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	// Destroy Vulkan swapchain/window resources first
	cleanupVulkanWindow();

	// Destroy Vulkan device, instance, and global resources
	cleanupVulkan();

	// Destroy GLFW window and terminate GLFW system
	glfwDestroyWindow(m_windowHandle);
	glfwTerminate();

	// Mark application as no longer running
	g_applicationRunning = false;
}

// Main application loop: handles events, updates layers, renders ImGui, and presents frames
void Application::run()
{
	m_isRunning = true;

	ImGui_ImplVulkanH_Window* wd = &g_mainWindowData;
	ImVec4 clearColor = ImVec4(0.45f, 0.55f, 0.60f, 1.0f);
	ImGuiIO& io = ImGui::GetIO();

	while (!glfwWindowShouldClose(m_windowHandle) && m_isRunning)
	{
		// Process OS/window system events
		glfwPollEvents();

		// Update all application layers
		for (auto& layer : m_layerStack)
			layer->onUpdate(m_timeStep);

		// Handle swapchain recreation (resize, format change, etc.)
		if (g_swapChainRebuild)
		{
			int width, height;
			glfwGetFramebufferSize(m_windowHandle, &width, &height);

			// Only rebuild if window has valid dimensions
			if (width > 0 && height > 0)
			{
				ImGui_ImplVulkan_SetMinImageCount(g_minImageCount);
				ImGui_ImplVulkanH_CreateOrResizeWindow(g_instance, g_physicalDevice, g_device, &g_mainWindowData, g_queueFamily, g_allocator, width, height, g_minImageCount);
				g_mainWindowData.FrameIndex = 0; // Reset frame indexing after swapchain rebuild

				// Reinitialize per-frame command tracking
				s_allocatedCommandBuffers.clear();
				s_allocatedCommandBuffers.resize(g_mainWindowData.ImageCount);

				g_swapChainRebuild = false;
			}
		}

		// Start new ImGui frame
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		{
			static bool dockspaceBuilt = false;

			// Dockspace configuration flags
			static ImGuiDockNodeFlags dockspaceFlags = ImGuiDockNodeFlags_PassthruCentralNode | ImGuiDockNodeFlags_NoTabBar;

			ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDocking;

			if (m_menubarCallback)
				windowFlags |= ImGuiWindowFlags_MenuBar;

			const ImGuiViewport* viewport = ImGui::GetMainViewport();

			// Make dockspace fill entire main viewport
			ImGui::SetNextWindowPos(viewport->WorkPos);
			ImGui::SetNextWindowSize(viewport->WorkSize);
			ImGui::SetNextWindowViewport(viewport->ID);

			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

			windowFlags |= ImGuiWindowFlags_NoTitleBar |
				ImGuiWindowFlags_NoCollapse |
				ImGuiWindowFlags_NoResize |
				ImGuiWindowFlags_NoMove |
				ImGuiWindowFlags_NoBringToFrontOnFocus |
				ImGuiWindowFlags_NoNavFocus;

			if (dockspaceFlags & ImGuiDockNodeFlags_PassthruCentralNode)
				windowFlags |= ImGuiWindowFlags_NoBackground;

			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
			ImGui::Begin("Dockspace Demo", nullptr, windowFlags);
			ImGui::PopStyleVar();
			ImGui::PopStyleVar(2);

			ImGuiIO& io = ImGui::GetIO();

			// Main docking root
			if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
			{
				ImGuiID dockspaceID = ImGui::GetID("VulkanAppDockspace");

				ImGui::DockSpace(dockspaceID, ImVec2(0.0f, 0.0f), dockspaceFlags);

				// Build initial docking layout once
				if (!dockspaceBuilt)
				{
					dockspaceBuilt = true;

					ImGui::DockBuilderRemoveNode(dockspaceID);
					ImGui::DockBuilderAddNode(dockspaceID, ImGuiDockNodeFlags_DockSpace | ImGuiDockNodeFlags_NoTabBar);
					ImGui::DockBuilderSetNodeSize(dockspaceID, viewport->WorkSize);

					ImGuiID dock_main = dockspaceID;

					// Right panel (tools / inspector)
					ImGuiID dock_right = ImGui::DockBuilderSplitNode(
						dock_main,
						ImGuiDir_Right,
						0.25f,
						nullptr,
						&dock_main
					);

					// Bottom panel (logs / status)
					ImGuiID dock_bottom = ImGui::DockBuilderSplitNode(
						dock_main,
						ImGuiDir_Down,
						0.03f,
						nullptr,
						&dock_main
					);

					ImGui::DockBuilderDockWindow("Viewport", dock_main);
					ImGui::DockBuilderDockWindow("Sidebar", dock_right);
					ImGui::DockBuilderDockWindow("Bottom Bar", dock_bottom);

					ImGui::DockBuilderFinish(dockspaceID);
				}
			}

			// Menu bar callback
			if (m_menubarCallback)
			{
				if (ImGui::BeginMenuBar())
				{
					m_menubarCallback();
					ImGui::EndMenuBar();
				}
			}

			// Render all UI layers
			for (auto& layer : m_layerStack)
				layer->onUIRender();

			ImGui::End();
		}

		// Finalize ImGui frame and fetch draw data
		ImGui::Render();
		ImDrawData* mainDrawData = ImGui::GetDrawData();

		// Detect minimized window (skip rendering)
		const bool mainIsMinimized = (mainDrawData->DisplaySize.x <= 0.0f || mainDrawData->DisplaySize.y <= 0.0f);

		// Set clear color for swapchain image
		wd->ClearValue.color.float32[0] = clearColor.x * clearColor.w;
		wd->ClearValue.color.float32[1] = clearColor.y * clearColor.w;
		wd->ClearValue.color.float32[2] = clearColor.z * clearColor.w;
		wd->ClearValue.color.float32[3] = clearColor.w;

		// Render frame if window is visible
		if (!mainIsMinimized)
			renderFrame(wd, mainDrawData);

		// Render additional platform windows (multi-viewport)
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
		}

		// Present final image
		if (!mainIsMinimized)
			framePresent(wd);

		// Update timing (delta time clamped for stability)
		float time = getTime();
		m_frameTime = time - m_lastFrameTime;
		m_timeStep = glm::min<float>(m_frameTime, 0.0333f);
		m_lastFrameTime = time;
	}
}

// Allocates and begins a transient Vulkan command buffer for the current frame
VkCommandBuffer Application::getCommandBuffer(bool begin)
{
	ImGui_ImplVulkanH_Window* wd = &g_mainWindowData;

	// Command pool tied to the current swapchain frame
	VkCommandPool commandPool = wd->Frames[wd->FrameIndex].CommandPool;

	// Allocation info for a single primary command buffer
	VkCommandBufferAllocateInfo cmdBufAllocInfo = {};
	cmdBufAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmdBufAllocInfo.commandPool = commandPool;
	cmdBufAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cmdBufAllocInfo.commandBufferCount = 1;

	// Track allocated command buffer for later cleanup
	VkCommandBuffer& commandBuffer = s_allocatedCommandBuffers[wd->FrameIndex].emplace_back();

	// Allocate command buffer from Vulkan
	auto err = vkAllocateCommandBuffers(g_device, &cmdBufAllocInfo, &commandBuffer);

	// Begin recording immediately (one-time submission pattern)
	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	err = vkBeginCommandBuffer(commandBuffer, &beginInfo);
	check_vk_result(err);

	return commandBuffer;
}

// Submits a one-time command buffer and blocks until GPU execution is complete
void Application::flushCommandBuffer(VkCommandBuffer commandBuffer)
{
	const uint64_t DEFAULT_FENCE_TIMEOUT = 100000000000;

	// Finalize recording of the command buffer
	VkSubmitInfo endInfo = {};
	endInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	endInfo.commandBufferCount = 1;
	endInfo.pCommandBuffers = &commandBuffer;
	auto err = vkEndCommandBuffer(commandBuffer);
	check_vk_result(err);

	// Create a temporary fence to wait for GPU completion
	VkFenceCreateInfo fenceCreateInfo = {};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.flags = 0;

	VkFence fence;
	err = vkCreateFence(g_device, &fenceCreateInfo, nullptr, &fence);
	check_vk_result(err);

	// Submit command buffer for execution
	err = vkQueueSubmit(g_queue, 1, &endInfo, fence);
	check_vk_result(err);

	// Block CPU until GPU finishes executing the command buffer
	err = vkWaitForFences(g_device, 1, &fence, VK_TRUE, DEFAULT_FENCE_TIMEOUT);
	check_vk_result(err);

	// Cleanup temporary synchronization primitive
	vkDestroyFence(g_device, fence, nullptr);
}

// Returns application runtime time in seconds (GLFW-based high resolution timer)
float Application::getTime()
{
	return (float)glfwGetTime();
}

// Returns the global Vulkan instance used by the application
VkInstance Application::getInstance()
{
	return g_instance;
}

// Returns the selected Vulkan physical device (GPU)
VkPhysicalDevice Application::getPhysicalDevice()
{
	return g_physicalDevice;
}

// Returns the logical Vulkan device used for all GPU operations
VkDevice Application::getDevice()
{
	return g_device;
}

// Queues a deferred resource destruction callback for the current frame.
// This ensures GPU is finished using resources before they are freed
void Application::submitResourceFree(std::function<void()>&& func)
{
	s_resourceFreeQueue[s_currentFrameIndex].emplace_back(func);
}
