#pragma once

//TODO: Will move this to precompiled header in the future
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <fstream>

#include "glm/vec2.hpp"
#include "glm/vec3.hpp"

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#	include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

struct Vertex
{
	glm::vec2 pos;
	glm::vec3 color;

	static vk::VertexInputBindingDescription getBindingDescription()
	{
		return {0, sizeof(Vertex), vk::VertexInputRate::eVertex};
	}

	static std::array<vk::VertexInputAttributeDescription, 2> getAttributeDescriptions()
	{
		return {
			vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, pos)),
			vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color))};
	}
};

const std::vector<Vertex> vertices = {
	   {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
	{{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
	{{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
	{{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}};

const std::vector<uint16_t> indices = {
	0, 1, 2, 2, 3, 0
};

class Application
{
public:
	Application();
	virtual ~Application();

    virtual void Run();
	void MainLoop();
	void Cleanup();
private:

	//Window
	void InitializeWindow();

	// Vulkan
	void InitializeVulkan();
	void CreateInstance();
	void SetupDebugMessenger();
	void CreateSurface();
	void PickPhysicalDevice();
	void CreateLogicalDevice();
	void CreateSwapChain();
	void CreateImageViews();
	void CreateGraphicsPipeline();
	void CreateCommandPool();
	void CreateVertexBuffer();
	void CreateIndexBuffer();
	void CreateBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, vk::raii::Buffer& buffer, vk::raii::DeviceMemory& bufferMemory);
	void CreateCommandBuffer();
	void CreateSyncObjects();
	void drawFrame();
	void recordCommandBuffer(uint32_t imageIndex);
	void transition_image_layout(uint32_t imageIndex, vk::ImageLayout old_layout, vk::ImageLayout new_layout,
	                             vk::AccessFlags2 src_access_mask, vk::AccessFlags2 dst_access_mask,
	                             vk::PipelineStageFlags2 src_stage_mask, vk::PipelineStageFlags2 dst_stage_mask);


	//helpers function- vulkan
	vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities);

	static uint32_t chooseSwapMinImageCount(vk::SurfaceCapabilitiesKHR const& surfaceCapabilities)
	{
		auto minImageCount = std::max(3u, surfaceCapabilities.minImageCount);
		if ((0 < surfaceCapabilities.maxImageCount) && (surfaceCapabilities.maxImageCount < minImageCount))
		{
			minImageCount = surfaceCapabilities.maxImageCount;
		}
		return minImageCount;
	}

	static vk::SurfaceFormatKHR chooseSwapSurfaceFormat(std::vector<vk::SurfaceFormatKHR> const& availableFormats)
	{
		assert(!availableFormats.empty());
		const auto formatIt = std::ranges::find_if(
			availableFormats,
			[](const auto& format) { return format.format == vk::Format::eB8G8R8A8Srgb && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear; });
		return formatIt != availableFormats.end() ? *formatIt : availableFormats[0];
	}

	static vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes)
	{
		assert(std::ranges::any_of(availablePresentModes, [](auto presentMode) { return presentMode == vk::PresentModeKHR::eFifo; }));
		return std::ranges::any_of(availablePresentModes,
			[](const vk::PresentModeKHR value) { return vk::PresentModeKHR::eMailbox == value; }) ?
			vk::PresentModeKHR::eMailbox :
			vk::PresentModeKHR::eFifo;
	}

	void copyBuffer(vk::raii::Buffer& srcBuffer, vk::raii::Buffer& dstBuffer, vk::DeviceSize size)
	{
		vk::CommandBufferAllocateInfo allocInfo;
		allocInfo.commandPool = VulkanCommandPool;
		allocInfo.level = vk::CommandBufferLevel::ePrimary;
		allocInfo.commandBufferCount = 1;

		vk::raii::CommandBuffer commandCopyBuffer = std::move(VulkanLogicalDevice.allocateCommandBuffers(allocInfo).front());

		vk::CommandBufferBeginInfo CommandBufferBeginInfo;
		CommandBufferBeginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

		commandCopyBuffer.begin(CommandBufferBeginInfo);
		commandCopyBuffer.copyBuffer(srcBuffer, dstBuffer, vk::BufferCopy(0, 0, size));
		commandCopyBuffer.end();

		vk::SubmitInfo SubmitInfo;
		SubmitInfo.commandBufferCount = 1;
		SubmitInfo.pCommandBuffers = &*commandCopyBuffer;

		VulkanGraphicsQueue.submit(SubmitInfo, nullptr);
		VulkanGraphicsQueue.waitIdle();
	}

	std::vector<char const*> getRequiredExtensions();

	static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT severity, vk::DebugUtilsMessageTypeFlagsEXT type, const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData, void*)
	{
		if (severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eError || severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning)
		{
			std::cerr << "validation layer: type " << to_string(type) << " msg: " << pCallbackData->pMessage << std::endl;
		}

		return vk::False;
	}

	//ShaderManager
	[[nodiscard]] vk::raii::ShaderModule CreateShaderModule(const std::vector<char>& code) const
	{
		vk::ShaderModuleCreateInfo CreateInfo;
		CreateInfo.codeSize = code.size() * sizeof(char);
		CreateInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

		vk::raii::ShaderModule     ShaderModule{ VulkanLogicalDevice, CreateInfo };

		return ShaderModule;
	}

	//FileManager
	static std::vector<char> ReadFile(const std::string& filename)
	{
		std::ifstream file(filename, std::ios::ate | std::ios::binary);
		if (!file.is_open())
		{
			throw std::runtime_error("Failed to open file!");
		}
		std::vector<char> buffer(file.tellg());
		file.seekg(0, std::ios::beg);
		file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
		file.close();
		return buffer;
	}

	uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties)
	{
		vk::PhysicalDeviceMemoryProperties memProperties = VulkanPhysicalDevice.getMemoryProperties();

		for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
		{
			if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
			{
				return i;
			}
		}

		throw std::runtime_error("failed to find suitable memory type!");
	}
protected:
	bool bIsApplicationRunning = true;

private:
	GLFWwindow* Window;

	//Vulkan
	vk::raii::Context  VulkanContext;
	vk::raii::Instance VulkanInstance = nullptr;
	vk::raii::DebugUtilsMessengerEXT VulkanDebugMessenger = nullptr;
	vk::raii::PhysicalDevice VulkanPhysicalDevice = nullptr;
	vk::raii::Device VulkanLogicalDevice = nullptr;
	vk::raii::Queue VulkanGraphicsQueue = nullptr;
	vk::raii::SurfaceKHR VulkanSurface = nullptr;
	vk::raii::SwapchainKHR           VulkanSwapChain = nullptr;
	std::vector<vk::Image>           VulkanSwapChainImages;
	vk::SurfaceFormatKHR             VulkanSwapChainSurfaceFormat;
	vk::Extent2D                     VulkanSwapChainExtent;
	std::vector<vk::raii::ImageView> VulkanSwapChainImageViews;
	uint32_t                         queueIndex = ~0;
	vk::raii::PipelineLayout VulkanPipelineLayout = nullptr;
	vk::raii::Pipeline       VulkanGraphicsPipeline = nullptr;
	vk::raii::CommandPool VulkanCommandPool = nullptr;
	vk::raii::CommandBuffer  VulkanCommandBuffer = nullptr;
	vk::raii::Semaphore VulkanPresentCompleteSemaphore = nullptr;
	vk::raii::Semaphore VulkanRenderFinishedSemaphore = nullptr;
	vk::raii::Fence     VulkanDrawFence = nullptr;
	vk::raii::Buffer vertexBuffer = nullptr;
	vk::raii::DeviceMemory vertexBufferMemory = nullptr;
	vk::raii::Buffer indexBuffer = nullptr;
	vk::raii::DeviceMemory indexBufferMemory = nullptr;

	std::vector<const char*> VulkanRequiredDeviceExtension = { vk::KHRSwapchainExtensionName };
};
