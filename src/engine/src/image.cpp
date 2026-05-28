// Engine
#include <image.h>
#include <application.h>

// ImGui
#include <imgui.h>
#include <backends/imgui_impl_vulkan.h>

// STB
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

namespace Utils
{
	// Finds a suitable Vulkan memory type index that matches required properties
	// and is supported by the physical device.
	static uint32_t getVulkanMemoryType(VkMemoryPropertyFlags properties, uint32_t type_bits)
	{
		VkPhysicalDeviceMemoryProperties props;
		vkGetPhysicalDeviceMemoryProperties(Application::getPhysicalDevice(), &props);

		// Iterate over all memory types exposed by the GPU
		for (uint32_t i = 0; i < props.memoryTypeCount; i++)
		{
			// Check if memory type supports required properties and is allowed by type_bits
			if ((props.memoryTypes[i].propertyFlags & properties) == properties && type_bits & (1 << i))
			{
				return i;
			}
		}

		// Invalid case: no suitable memory type found
		return 0xffffffff;
	}

	// Returns number of bytes per pixel for a given image format
	static uint32_t bytesPerPixel(ImageFormat format)
	{
		switch (format)
		{
			case ImageFormat::RGBA:		return 4;		// 8-bit per channel
			case ImageFormat::RGBA32F:  return 16;		// 32-bit float per channel
		}

		return 0;
	}

	// Converts engine image format into Vulkan format enum
	static VkFormat convertFormatToVulkan(ImageFormat format)
	{
		switch (format)
		{
			case ImageFormat::RGBA:		return VK_FORMAT_R8G8B8A8_UNORM;
			case ImageFormat::RGBA32F:  return VK_FORMAT_R32G32B32A32_SFLOAT;
		}
		
		// Invalid / unsupported format fallback
		return (VkFormat)0;
	}
}

Image::Image(std::string_view path) : m_filePath(path)
{
	int width, height, channels;
	uint8_t* data = nullptr;

	// Determine whether the image is HDR (floating-point data) or standard LDR
	if (stbi_is_hdr(m_filePath.c_str()))
	{
		// Load HDR image and force 4 channels (RGBA)
		data = (uint8_t*)stbi_load(m_filePath.c_str(), &width, &height, &channels, 4);
		m_format = ImageFormat::RGBA32F;
	}

	else
	{
		// Load standard 8-bit image and force 4 channels (RGBA)
		data = stbi_load(m_filePath.c_str(), &width, &height, &channels, 4);
		m_format = ImageFormat::RGBA;
	}

	// Store image dimensions
	m_width = width;
	m_height = height;

	// Allocate image and upload CPU data
	allocateMemory(m_width * m_height * Utils::bytesPerPixel(m_format));
	setData(data);

	stbi_image_free(data);
}

Image::Image(uint32_t w, uint32_t h, ImageFormat f, const void* data)
	: m_width(w), m_height(h), m_format(f)
{
	// Allocate image, memory, view, and sampler
	allocateMemory(m_width * m_height * Utils::bytesPerPixel(m_format));

	// If initial pixel data is provided, upload it to the image
	if (data) { setData(data); }

}

Image::~Image()
{
	// Ensure all Vulkan resources are properly released
	release();
}

// Downloads a Vulkan image to CPU memory and writes it as a PNG file using stb_image_write.
void Image::savePNG(const std::string& path)
{
	VkDevice device = Application::getDevice();

	// Total image size in bytes (assumes tightly packed RGBA-style layout)
	VkDeviceSize imageSize = m_width * m_height * Utils::bytesPerPixel(m_format);

	VkBuffer dstBuffer;
	VkDeviceMemory dstMemory;

	// Flip image vertically for conventional image output (top-left origin)
	stbi_flip_vertically_on_write(1);

	// Create readback buffer
	{
		VkBufferCreateInfo info{};
		info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		info.size = imageSize;
		info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;

		vkCreateBuffer(device, &info, nullptr, &dstBuffer);

		// Allocate host-visible memory for readback
		VkMemoryRequirements reqs;
		vkGetBufferMemoryRequirements(device, dstBuffer, &reqs);

		VkMemoryAllocateInfo alloc{};
		alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		alloc.allocationSize = reqs.size;
		alloc.memoryTypeIndex =
			Utils::getVulkanMemoryType(
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
				VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				reqs.memoryTypeBits
			);

		vkAllocateMemory(device, &alloc, nullptr, &dstMemory);
		vkBindBufferMemory(device, dstBuffer, dstMemory, 0);
	}

	// Copy image -> buffer
	{
		VkCommandBuffer cmd =
			Application::getCommandBuffer(true);

		// Transition image for reading from shader layout to transfer source
		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		barrier.image = m_image;

		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.layerCount = 1;

		vkCmdPipelineBarrier(
			cmd,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			0,
			0, nullptr,
			0, nullptr,
			1, &barrier
		);

		// Define region to copy from image into buffer
		VkBufferImageCopy copyRegion{};
		copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copyRegion.imageSubresource.layerCount = 1;

		copyRegion.imageExtent = {
			m_width,
			m_height,
			1
		};

		// Perform image readback into buffer
		vkCmdCopyImageToBuffer(
			cmd,
			m_image,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			dstBuffer,
			1,
			&copyRegion
		);

		// Submit and wait for completion
		Application::flushCommandBuffer(cmd);
	}

	//  Map buffer and write PNG
	void* mapped;
	vkMapMemory(device, dstMemory, 0, imageSize, 0, &mapped);

	stbi_write_png(
		path.c_str(),
		m_width,
		m_height,
		4,					// assuming RGBA8 layout
		mapped,
		m_width * 4			// row pitch
	);

	vkUnmapMemory(device, dstMemory);

	// Cleanup temporary readback resources
	vkDestroyBuffer(device, dstBuffer, nullptr);
	vkFreeMemory(device, dstMemory, nullptr);
}

// Allocates a Vulkan image, creates its memory, image view, sampler,
// and registers it as an ImGui texture descriptor.
void Image::allocateMemory(uint64_t size)
{
	VkDevice device = Application::getDevice();

	VkResult err;

	// Convert internal format to Vulkan format
	VkFormat vulkanFormat = Utils::convertFormatToVulkan(m_format);

	// Create image resource
	{
		VkImageCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		info.imageType = VK_IMAGE_TYPE_2D;
		info.format = vulkanFormat;
		info.extent.width = m_width;
		info.extent.height = m_height;
		info.extent.depth = 1;
		info.mipLevels = 1;
		info.arrayLayers = 1;
		info.samples = VK_SAMPLE_COUNT_1_BIT;
		info.tiling = VK_IMAGE_TILING_OPTIMAL; // Image can be sampled in shaders and used as transfer destination
		info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		err = vkCreateImage(device, &info, nullptr, &m_image);
		check_vk_result(err);

		// Query required memory size/alignment
		VkMemoryRequirements reqs;
		vkGetImageMemoryRequirements(device, m_image, &reqs);

		// Allocate device-local memory for the image
		VkMemoryAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = reqs.size;
		allocInfo.memoryTypeIndex = Utils::getVulkanMemoryType(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, reqs.memoryTypeBits);
		err = vkAllocateMemory(device, &allocInfo, nullptr, &m_memory);
		check_vk_result(err);

		// Bind memory to image
		err = vkBindImageMemory(device, m_image, m_memory, 0);
		check_vk_result(err);
	}

	// Create image view
	{
		VkImageViewCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		info.image = m_image;
		info.viewType = VK_IMAGE_VIEW_TYPE_2D;
		info.format = vulkanFormat;

		// Color texture view covering full image
		info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		info.subresourceRange.levelCount = 1;
		info.subresourceRange.layerCount = 1;

		err = vkCreateImageView(device, &info, nullptr, &m_imageView);
		check_vk_result(err);
	}

	// Create sampler
	{
		VkSamplerCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

		// Linear filtering for smooth sampling
		info.magFilter = VK_FILTER_LINEAR;
		info.minFilter = VK_FILTER_LINEAR;
		info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

		// Texture wrapping mode
		info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

		// LOD range (wide range allows flexibility, though unusual in practice)
		info.minLod = -1000;
		info.maxLod = 1000;

		// No anisotropic filtering
		info.maxAnisotropy = 1.0f;

		VkResult err = vkCreateSampler(device, &info, nullptr, &m_sampler);
		check_vk_result(err);
	}

	// Register texture with ImGui Vulkan backend as a descriptor set
	m_descriptorSet = (VkDescriptorSet)ImGui_ImplVulkan_AddTexture(m_sampler, m_imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

// Releases all Vulkan resources associated with this image.
// Uses deferred deletion to ensure GPU is finished with resources
// before they are actually destroyed.
void Image::release()
{
	// Queue GPU-safe destruction of Vulkan resources
	Application::submitResourceFree([sampler = m_sampler, imageView = m_imageView, image = m_image,
		memory = m_memory, stagingBuffer = m_stagingBuffer, stagingBufferMemory = m_stagingBufferMemory]()
		{
			VkDevice device = Application::getDevice();

			// Destroy sampler (texture sampling state)
			vkDestroySampler(device, sampler, nullptr);

			// Destroy image view (descriptor view into image)
			vkDestroyImageView(device, imageView, nullptr);

			// Destroy image resource
			vkDestroyImage(device, image, nullptr);

			// Free device memory backing the image
			vkFreeMemory(device, memory, nullptr);

			// Destroy staging buffer used for CPU uploads
			vkDestroyBuffer(device, stagingBuffer, nullptr);

			// Free memory backing staging buffer
			vkFreeMemory(device, stagingBufferMemory, nullptr);
		});

	// Clear local handles to prevent accidental reuse after release
	m_sampler = nullptr;
	m_imageView = nullptr;
	m_image = nullptr;
	m_memory = nullptr;
	m_stagingBuffer = nullptr;
	m_stagingBufferMemory = nullptr;
}

// Uploads raw pixel data into a Vulkan image using a staging buffer
// and transitions the image through the proper layout states.
void Image::setData(const void* data)
{
	VkDevice device = Application::getDevice();

	// Total upload size in bytes (width * height * bytes per pixel)
	size_t uploadSize = m_width * m_height * Utils::bytesPerPixel(m_format);

	VkResult err;

	// Create staging buffer (CPU-visible upload buffer)
	if (!m_stagingBuffer)
	{
		{
			VkBufferCreateInfo bufferInfo = {};
			bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			bufferInfo.size = uploadSize;
			bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			err = vkCreateBuffer(device, &bufferInfo, nullptr, &m_stagingBuffer);
			check_vk_result(err);

			// Query memory requirements for the staging buffer
			VkMemoryRequirements reqs;
			vkGetBufferMemoryRequirements(device, m_stagingBuffer, &reqs);

			// Store aligned allocation size (may be larger than uploadSize)
			m_alignedSize = reqs.size;

			// Allocate host-visible memory for staging buffer
			VkMemoryAllocateInfo allocInfo = {};
			allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			allocInfo.allocationSize = reqs.size;
			allocInfo.memoryTypeIndex = Utils::getVulkanMemoryType(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, reqs.memoryTypeBits);
			err = vkAllocateMemory(device, &allocInfo, nullptr, &m_stagingBufferMemory);
			check_vk_result(err);

			// Bind memory to buffer
			err = vkBindBufferMemory(device, m_stagingBuffer, m_stagingBufferMemory, 0);
			check_vk_result(err);
		}
	}

	// Copy CPU data into staging buffer
	{
		char* map = NULL;

		// Map GPU memory into CPU address space
		err = vkMapMemory(device, m_stagingBufferMemory, 0, m_alignedSize, 0, (void**)(&map));
		check_vk_result(err);

		// Copy pixel data into mapped memory
		memcpy(map, data, uploadSize);

		// Ensure writes are visible to GPU
		VkMappedMemoryRange range[1] = {};
		range[0].sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		range[0].memory = m_stagingBufferMemory;
		range[0].size = m_alignedSize;
		err = vkFlushMappedMemoryRanges(device, 1, range);
		check_vk_result(err);

		// Unmap after upload
		vkUnmapMemory(device, m_stagingBufferMemory);
	}

	// Record GPU copy + layout transitions
	{
		// Get one-time command buffer for immediate submission
		VkCommandBuffer cmdBuffer = Application::getCommandBuffer(true);

		// Transition image: undefined -> transfer destination
		VkImageMemoryBarrier cpyBarrier = {};
		cpyBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		cpyBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		cpyBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		cpyBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		cpyBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		cpyBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		cpyBarrier.image = m_image;
		cpyBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		cpyBarrier.subresourceRange.levelCount = 1;
		cpyBarrier.subresourceRange.layerCount = 1;
		vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &cpyBarrier);

		// Copy buffer data into image
		VkBufferImageCopy region = {};
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.layerCount = 1;
		region.imageExtent.width = m_width;
		region.imageExtent.height = m_height;
		region.imageExtent.depth = 1;
		vkCmdCopyBufferToImage(cmdBuffer, m_stagingBuffer, m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

		// Transition image: transfer destination -> shader readable
		VkImageMemoryBarrier useBarrier = {};
		useBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		useBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		useBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		useBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		useBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		useBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		useBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		useBarrier.image = m_image;
		useBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		useBarrier.subresourceRange.levelCount = 1;
		useBarrier.subresourceRange.layerCount = 1;
		vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &useBarrier);

		// Submit and execute command buffer immediately
		Application::flushCommandBuffer(cmdBuffer);
	}
}

// Resizes the image buffer to a new resolution
// If size hasn't changed, early-out to avoid reallocating memory
void Image::resize(uint32_t width, uint32_t height)
{
	// Skip work if image already matches requested size and is allocated
	if (m_image && m_width == width && m_height == height)
		return;

	// Update stored dimensions
	m_width = width;
	m_height = height;

	// Free previous GPU/CPU image memory (if any)
	release();

	// Allocate new buffer based on pixel count and format byte size
	allocateMemory(m_width * m_height * Utils::bytesPerPixel(m_format));
}
