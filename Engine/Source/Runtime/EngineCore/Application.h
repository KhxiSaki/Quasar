#pragma once

//TODO: Will move this to precompiled header in the future
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <fstream>

#include "ModuleManager.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#	include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <stb_image.h>

#include <tiny_obj_loader.h>

struct Vertex
{
	glm::vec3 pos;
	glm::vec3 color;
	glm::vec2 texCoord;

	static vk::VertexInputBindingDescription getBindingDescription()
	{
		return {0, sizeof(Vertex), vk::VertexInputRate::eVertex};
	}

	static std::array<vk::VertexInputAttributeDescription, 3> getAttributeDescriptions()
	{
		return {
			vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, pos)),
			vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color)),
			vk::VertexInputAttributeDescription(2, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, texCoord))};
	}

	bool operator==(const Vertex& other) const {
		return pos == other.pos && color == other.color && texCoord == other.texCoord;
	}
};

template <>
struct std::hash<Vertex>
{
	size_t operator()(Vertex const& vertex) const noexcept
	{
		return ((hash<glm::vec3>()(vertex.pos) ^ (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^ (hash<glm::vec2>()(vertex.texCoord) << 1);
	}
};
struct UniformBufferObject {
	alignas(16) glm::mat4 model;
	alignas(16) glm::mat4 view;
	alignas(16) glm::mat4 proj;
};

class Application
{
public:
	Application();
	virtual ~Application();

    virtual void Run();
	void MainLoop();
	void RecreateSwapChain();
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
	vk::raii::ImageView CreateImageView(vk::raii::Image& image, vk::Format format, vk::ImageAspectFlags aspectFlags);
	void CreateImageViews();
	void CreateDescriptorSetLayout();
	void CreateGraphicsPipeline();
	void CreateCommandPool();
	void CreateDepthResources();
	void CreateTextureImage();
	void CreateImage(uint32_t width, uint32_t height, vk::Format format, vk::ImageTiling tiling,
	                 vk::ImageUsageFlags usage,
	                 vk::MemoryPropertyFlags properties, vk::raii::Image& image, vk::raii::DeviceMemory& imageMemory);
	void CreateTextureImageView();
	void CreateTextureSampler();
	void LoadModel();
	void CreateVertexBuffer();
	void CreateIndexBuffer();
	void CreateUniformBuffers();
	void UpdateUniformBuffer(uint32_t currentImage);
	void CreateBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, vk::raii::Buffer& buffer, vk::raii::DeviceMemory& bufferMemory);
	void CreateDescriptorPool();
	void CreateDescriptorSets();
	void CreateCommandBuffers();
	void CreateSyncObjects();
	void drawFrame();
	void recordCommandBuffer(uint32_t imageIndex);
	void transition_image_layout(vk::Image               image, vk::ImageLayout old_layout, vk::ImageLayout new_layout,
	                             vk::AccessFlags2 src_access_mask, vk::AccessFlags2 dst_access_mask,
	                             vk::PipelineStageFlags2 src_stage_mask, vk::PipelineStageFlags2 dst_stage_mask, vk::ImageAspectFlags    image_aspect_flags);
	void transitionImageLayout(const vk::raii::Image& image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout);


	//helpers function- vulkan
	vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities);
	void CleanupSwapChain();

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

		vk::raii::CommandBuffer       commandCopyBuffer = std::move(VulkanLogicalDevice.allocateCommandBuffers(allocInfo).front());

		vk::CommandBufferBeginInfo CommandBufferBeginInfo;
		CommandBufferBeginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

		commandCopyBuffer.begin(CommandBufferBeginInfo);

		vk::BufferCopy copybuffer;
		copybuffer.size = size;

		commandCopyBuffer.copyBuffer(*srcBuffer, *dstBuffer, copybuffer);
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

	static void framebufferResizeCallback(GLFWwindow* window, int width, int height)
	{
		auto app = static_cast<Application*>(glfwGetWindowUserPointer(window));
		app->framebufferResized = true;
	}

	//Texture
	vk::raii::CommandBuffer beginSingleTimeCommands()
	{
		vk::CommandBufferAllocateInfo allocInfo;
		allocInfo.commandPool = VulkanCommandPool;
		allocInfo.level = vk::CommandBufferLevel::ePrimary;
		allocInfo.commandBufferCount = 1;
	
		vk::raii::CommandBuffer commandBuffer = std::move(VulkanLogicalDevice.allocateCommandBuffers(allocInfo).front());

		vk::CommandBufferBeginInfo beginInfo;
		beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
		
		commandBuffer.begin(beginInfo);

		return commandBuffer;
	}

	void endSingleTimeCommands(vk::raii::CommandBuffer& commandBuffer)
	{
		commandBuffer.end();

		vk::SubmitInfo submitInfo;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &*commandBuffer;
	
		VulkanGraphicsQueue.submit(submitInfo, nullptr);
		VulkanGraphicsQueue.waitIdle();
	}

	void copyBufferToImage(const vk::raii::Buffer& buffer, vk::raii::Image& image, uint32_t width, uint32_t height) {
		vk::raii::CommandBuffer commandBuffer = beginSingleTimeCommands();

		vk::BufferImageCopy region;
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;
		region.imageSubresource = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 };
		region.imageOffset = vk::Offset3D{ 0, 0, 0 };
		region.imageExtent = vk::Extent3D{ width, height, 1 };

		commandBuffer.copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, { region });
		endSingleTimeCommands(commandBuffer);
	}

	// Depth Buffering (3D)
	vk::Format findSupportedFormat(const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features) 
	{
		for (const auto format : candidates) {
			vk::FormatProperties props = VulkanPhysicalDevice.getFormatProperties(format);

			if (tiling == vk::ImageTiling::eLinear && (props.linearTilingFeatures & features) == features) {
				return format;
			}
			if (tiling == vk::ImageTiling::eOptimal && (props.optimalTilingFeatures & features) == features) {
				return format;
			}
		}

		throw std::runtime_error("failed to find supported format!");
	}

	vk::Format findDepthFormat() {
		return findSupportedFormat(
			{ vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint },
			vk::ImageTiling::eOptimal,
			vk::FormatFeatureFlagBits::eDepthStencilAttachment
		);
	}

	bool hasStencilComponent(vk::Format format) {
		return format == vk::Format::eD32SfloatS8Uint || format == vk::Format::eD24UnormS8Uint;
	}
protected:
	void LoadPlugins();
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
	vk::raii::DescriptorSetLayout VulkanDescriptorSetLayout = nullptr;
	vk::raii::PipelineLayout VulkanPipelineLayout = nullptr;
	vk::raii::Pipeline       VulkanGraphicsPipeline = nullptr;
	vk::raii::CommandPool VulkanCommandPool = nullptr;
	std::vector < vk::raii::CommandBuffer>  VulkanCommandBuffers;
	std::vector < vk::raii::Semaphore> VulkanPresentCompleteSemaphores;
	std::vector < vk::raii::Semaphore> VulkanRenderFinishedSemaphores;
	vk::raii::Fence     VulkanDrawFence = nullptr;
	vk::raii::Buffer VulkanVertexBuffer = nullptr;
	vk::raii::DeviceMemory VulkanVertexBufferMemory = nullptr;
	vk::raii::Buffer VulkanIndexBuffer = nullptr;
	vk::raii::DeviceMemory VulkanIndexBufferMemory = nullptr;
	std::vector<vk::raii::Buffer> VulkanUniformBuffers;
	std::vector<vk::raii::DeviceMemory> VulkanUniformBuffersMemory;
	std::vector<void*> VulkanUniformBuffersMapped;
	std::vector<vk::raii::Fence>     inFlightFences;
	uint32_t                         frameIndex = 0;
	vk::raii::DescriptorPool VulkanDescriptorPool = nullptr;
	std::vector<vk::raii::DescriptorSet> VulkanDescriptorSets;

	bool framebufferResized = false;
	std::vector<const char*> VulkanRequiredDeviceExtension = { vk::KHRSwapchainExtensionName,
		vk::KHRSpirv14ExtensionName,
		vk::KHRSynchronization2ExtensionName };

	vk::raii::Image depthImage = nullptr;
	vk::raii::DeviceMemory depthImageMemory = nullptr;
	vk::raii::ImageView depthImageView = nullptr;

	//Texture
	vk::raii::Image textureImage = nullptr;
	vk::raii::DeviceMemory textureImageMemory = nullptr;
	vk::raii::ImageView textureImageView = nullptr;
	vk::raii::Sampler textureSampler = nullptr;

	//Model
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
};
