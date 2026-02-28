#pragma once

#include <vulkan/vulkan_raii.hpp>
#include <imgui.h>
#include <glm/glm.hpp>

#include <vector>
#include <array>

class ImGuiVulkanUtil {
private:
    vk::raii::Sampler sampler{ nullptr };
    vk::raii::Buffer vertexBuffer{ nullptr };
    vk::raii::Buffer indexBuffer{ nullptr };
    vk::raii::DeviceMemory vertexBufferMemory{ nullptr };
    vk::raii::DeviceMemory indexBufferMemory{ nullptr };
    uint32_t vertexCount = 0;
    uint32_t indexCount = 0;
    vk::DeviceSize maxVertexBufferSize = 0;
    vk::DeviceSize maxIndexBufferSize = 0;
    vk::raii::Image fontImage{ nullptr };
    vk::raii::DeviceMemory fontImageMemory{ nullptr };
    vk::raii::ImageView fontImageView{ nullptr };

    vk::raii::PipelineCache pipelineCache{ nullptr };
    vk::raii::PipelineLayout pipelineLayout{ nullptr };
    vk::raii::Pipeline pipeline{ nullptr };
    vk::raii::DescriptorPool descriptorPool{ nullptr };
    vk::raii::DescriptorSetLayout descriptorSetLayout{ nullptr };
    vk::raii::DescriptorSet descriptorSet{ nullptr };

    vk::raii::Device* device = nullptr;
    vk::raii::PhysicalDevice* physicalDevice = nullptr;
    vk::raii::Queue* graphicsQueue = nullptr;
    vk::raii::CommandPool* commandPool = nullptr;
    uint32_t graphicsQueueFamily = 0;

    ImGuiStyle vulkanStyle;

    struct PushConstBlock {
        glm::vec2 scale;
        glm::vec2 translate;
    } pushConstBlock;

    bool needsUpdateBuffers = false;

    vk::Format colorFormat = vk::Format::eB8G8R8A8Unorm;

    void createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, vk::raii::Buffer& buffer, vk::raii::DeviceMemory& bufferMemory);
    void createImage(uint32_t width, uint32_t height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties, vk::raii::Image& image, vk::raii::DeviceMemory& imageMemory);
    vk::raii::ImageView createImageView(vk::raii::Image& image, vk::Format format, vk::ImageAspectFlags aspectFlags);
    void transitionImageLayout(vk::Image image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout);
    void copyBufferToImage(vk::raii::Buffer& buffer, vk::raii::Image& image, uint32_t width, uint32_t height);
    uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties);

public:
    ImGuiVulkanUtil();
    ~ImGuiVulkanUtil();

    void init(vk::raii::Device& device, vk::raii::PhysicalDevice& physicalDevice,
              vk::raii::Queue& graphicsQueue, vk::raii::CommandPool& commandPool, uint32_t graphicsQueueFamily);
    void setColorFormat(vk::Format format) { colorFormat = format; }
    void initialize(float width, float height);
    void updateDisplaySize(float width, float height);
    void initResources();
    void setStyle(uint32_t index);

    bool newFrame();
    void updateBuffers();
    void drawFrame(vk::raii::CommandBuffer& commandBuffer, vk::raii::ImageView& swapChainImageView, vk::Extent2D swapChainExtent);

    void handleKey(int key, int scancode, int action, int mods);
    bool getWantKeyCapture();
    void charPressed(uint32_t key);
    void handleMouseButton(int button, int action, int mods);
    void handleMousePosition(double xpos, double ypos);
    void handleMouseScroll(double xoffset, double yoffset);

    void cleanup();
};
