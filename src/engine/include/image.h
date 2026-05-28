#pragma once

// C++
#include <string>

// Vulkan
#include <vulkan/vulkan.h>

// Defines supported image pixel formats used for textures/framebuffers
enum class ImageFormat
{
    None = 0,   // Unspecified / invalid format
    RGBA,       // 8-bit per channel unsigned normalized (standard color texture)
    RGBA32F     // 32-bit float per channel (HDR / high precision rendering)
};

class Image
{
public:
    Image(std::string_view path);
    Image(uint32_t w, uint32_t h, ImageFormat f, const void* data = nullptr);
    ~Image();

    void setData(const void* data);

    VkDescriptorSet getDescriptorSet() const { return m_descriptorSet; }

    void resize(uint32_t width, uint32_t height);

    uint32_t getWidth() const { return m_width; }
    uint32_t getHeight() const { return m_height; }

    void savePNG(const std::string& path);

private:
    void allocateMemory(uint64_t size);
    void release();

private:
    uint32_t m_width = 0, m_height = 0;

    VkImage m_image = nullptr;
    VkImageView m_imageView = nullptr;
    VkDeviceMemory m_memory = nullptr;
    VkSampler m_sampler = nullptr;

    ImageFormat m_format = ImageFormat::None;

    VkBuffer m_stagingBuffer = nullptr;
    VkDeviceMemory m_stagingBufferMemory = nullptr;

    size_t m_alignedSize = 0;

    VkDescriptorSet m_descriptorSet = nullptr;
    std::string m_filePath;
};