#pragma once

#include <vulkan/vulkan_raii.hpp>

#include <cstdint>
#include <vector>

class SceneRenderTarget
{
public:
	SceneRenderTarget() = default;
	~SceneRenderTarget() = default;

	void create(vk::raii::Device& device, vk::raii::PhysicalDevice& physicalDevice,
		uint32_t width, uint32_t height, vk::Format colorFormat, vk::Format depthFormat);
	void destroy(vk::raii::Device& device);
	void resize(vk::raii::Device& device, vk::raii::PhysicalDevice& physicalDevice,
		uint32_t width, uint32_t height, vk::Format colorFormat, vk::Format depthFormat);

	vk::raii::Image& getColorImage() { return colorImage; }
	vk::raii::ImageView& getColorImageView() { return colorImageView; }
	vk::raii::Image& getDepthImage() { return depthImage; }
	vk::raii::ImageView& getDepthImageView() { return depthImageView; }
	uint32_t getWidth() const { return width; }
	uint32_t getHeight() const { return height; }

	vk::raii::Sampler sampler{ nullptr };
	vk::raii::DescriptorSetLayout descriptorSetLayout{ nullptr };
	vk::raii::DescriptorSet descriptorSet{ nullptr };

	void createSampler(vk::raii::Device& device);
	void createDescriptorSet(vk::raii::Device& device, vk::raii::DescriptorPool& descriptorPool);

	vk::raii::DescriptorSet& getDescriptorSet() { return descriptorSet; }
	VkDescriptorSet getVkDescriptorSet() { return *descriptorSet; }
	vk::raii::Sampler& getSampler() { return sampler; }

private:
	void createImage(vk::raii::Device& device, vk::raii::PhysicalDevice& physicalDevice,
		uint32_t w, uint32_t h, vk::Format format, vk::ImageTiling tiling,
		vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties,
		vk::raii::Image& image, vk::raii::DeviceMemory& imageMemory);

    vk::raii::ImageView createImageView(vk::raii::Image& image, vk::Format format, vk::ImageAspectFlags aspectFlags, vk::raii::Device& device);

	uint32_t findMemoryType(vk::raii::PhysicalDevice& physicalDevice, uint32_t typeFilter, vk::MemoryPropertyFlags properties);

	uint32_t width = 0;
	uint32_t height = 0;
	vk::Format colorFormat = vk::Format::eB8G8R8A8Unorm;
	vk::Format depthFormat = vk::Format::eD32Sfloat;

	vk::raii::Image colorImage{ nullptr };
	vk::raii::DeviceMemory colorImageMemory{ nullptr };
	vk::raii::ImageView colorImageView{ nullptr };

	vk::raii::Image depthImage{ nullptr };
	vk::raii::DeviceMemory depthImageMemory{ nullptr };
	vk::raii::ImageView depthImageView{ nullptr };
};
