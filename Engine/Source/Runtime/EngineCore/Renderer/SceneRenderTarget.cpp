#include "SceneRenderTarget.h"

#include <stdexcept>

void SceneRenderTarget::create(vk::raii::Device& device, vk::raii::PhysicalDevice& physicalDevice,
	uint32_t w, uint32_t h, vk::Format colorFmt, vk::Format depthFmt)
{
	width = w;
	height = h;
	colorFormat = colorFmt;
	depthFormat = depthFmt;

	createImage(device, physicalDevice, width, height, colorFormat, vk::ImageTiling::eOptimal,
		vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
		vk::MemoryPropertyFlagBits::eDeviceLocal, colorImage, colorImageMemory);

	colorImageView = createImageView(colorImage, colorFormat, vk::ImageAspectFlagBits::eColor, device);

	createImage(device, physicalDevice, width, height, depthFormat, vk::ImageTiling::eOptimal,
		vk::ImageUsageFlagBits::eDepthStencilAttachment,
		vk::MemoryPropertyFlagBits::eDeviceLocal, depthImage, depthImageMemory);

	depthImageView = createImageView(depthImage, depthFormat, vk::ImageAspectFlagBits::eDepth, device);
}

void SceneRenderTarget::destroy(vk::raii::Device& device)
{
	device.waitIdle();

	colorImageView = nullptr;
	depthImageView = nullptr;
	colorImage = nullptr;
	depthImage = nullptr;
	colorImageMemory = nullptr;
	depthImageMemory = nullptr;
}

void SceneRenderTarget::resize(vk::raii::Device& device, vk::raii::PhysicalDevice& physicalDevice,
	uint32_t w, uint32_t h, vk::Format colorFmt, vk::Format depthFmt)
{
	device.waitIdle();

	// Destroy old images only
	colorImageView = nullptr;
	depthImageView = nullptr;
	colorImage = nullptr;
	depthImage = nullptr;
	colorImageMemory = nullptr;
	depthImageMemory = nullptr;

	// Recreate images with new size
	width = w;
	height = h;
	colorFormat = colorFmt;
	depthFormat = depthFmt;

	createImage(device, physicalDevice, width, height, colorFormat, vk::ImageTiling::eOptimal,
		vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
		vk::MemoryPropertyFlagBits::eDeviceLocal, colorImage, colorImageMemory);

	colorImageView = createImageView(colorImage, colorFormat, vk::ImageAspectFlagBits::eColor, device);

	createImage(device, physicalDevice, width, height, depthFormat, vk::ImageTiling::eOptimal,
		vk::ImageUsageFlagBits::eDepthStencilAttachment,
		vk::MemoryPropertyFlagBits::eDeviceLocal, depthImage, depthImageMemory);

	depthImageView = createImageView(depthImage, depthFormat, vk::ImageAspectFlagBits::eDepth, device);

	// Update descriptor set with new image view
	vk::DescriptorImageInfo imageInfo{};
	imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
	imageInfo.imageView = *colorImageView;
	imageInfo.sampler = *sampler;

	vk::WriteDescriptorSet writeSet{};
	writeSet.dstSet = *descriptorSet;
	writeSet.descriptorCount = 1;
	writeSet.descriptorType = vk::DescriptorType::eCombinedImageSampler;
	writeSet.pImageInfo = &imageInfo;
	writeSet.dstBinding = 0;

	device.updateDescriptorSets({ writeSet }, {});
}

void SceneRenderTarget::createImage(vk::raii::Device& device, vk::raii::PhysicalDevice& physicalDevice,
	uint32_t w, uint32_t h, vk::Format format, vk::ImageTiling tiling,
	vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties,
	vk::raii::Image& image, vk::raii::DeviceMemory& imageMemory)
{
	vk::ImageCreateInfo imageInfo;
	imageInfo.imageType = vk::ImageType::e2D;
	imageInfo.format = format;
	imageInfo.extent = vk::Extent3D{ w, h, 1 };
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.samples = vk::SampleCountFlagBits::e1;
	imageInfo.tiling = tiling;
	imageInfo.usage = usage;
	imageInfo.sharingMode = vk::SharingMode::eExclusive;
	imageInfo.initialLayout = vk::ImageLayout::eUndefined;

	image = vk::raii::Image(device, imageInfo);

	vk::MemoryRequirements memRequirements = image.getMemoryRequirements();
	vk::MemoryAllocateInfo allocInfo;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties);

	imageMemory = vk::raii::DeviceMemory(device, allocInfo);
	image.bindMemory(*imageMemory, 0);
}

vk::raii::ImageView SceneRenderTarget::createImageView(vk::raii::Image& image, vk::Format format, vk::ImageAspectFlags aspectFlags, vk::raii::Device& device)
{
    vk::ImageViewCreateInfo viewInfo;
    viewInfo.image = *image;
    viewInfo.viewType = vk::ImageViewType::e2D;
    viewInfo.format = format;
    viewInfo.subresourceRange = { aspectFlags, 0, 1, 0, 1 };

    return vk::raii::ImageView(device, viewInfo);
}

uint32_t SceneRenderTarget::findMemoryType(vk::raii::PhysicalDevice& physicalDevice, uint32_t typeFilter, vk::MemoryPropertyFlags properties)
{
	vk::PhysicalDeviceMemoryProperties memProperties = physicalDevice.getMemoryProperties();

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
	{
		if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
		{
			return i;
		}
	}

	throw std::runtime_error("failed to find suitable memory type!");
}

void SceneRenderTarget::createSampler(vk::raii::Device& device)
{
	vk::SamplerCreateInfo samplerInfo{};
	samplerInfo.magFilter = vk::Filter::eLinear;
	samplerInfo.minFilter = vk::Filter::eLinear;
	samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
	samplerInfo.addressModeU = vk::SamplerAddressMode::eClampToEdge;
	samplerInfo.addressModeV = vk::SamplerAddressMode::eClampToEdge;
	samplerInfo.addressModeW = vk::SamplerAddressMode::eClampToEdge;
	samplerInfo.borderColor = vk::BorderColor::eFloatOpaqueWhite;

	sampler = device.createSampler(samplerInfo);
}

void SceneRenderTarget::createDescriptorSet(vk::raii::Device& device, vk::raii::DescriptorPool& descriptorPool)
{
	// Create descriptor set layout
	vk::DescriptorSetLayoutBinding binding;
	binding.binding = 0;
	binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
	binding.descriptorCount = 1;
	binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

	vk::DescriptorSetLayoutCreateInfo layoutInfo;
	layoutInfo.bindingCount = 1;
	layoutInfo.pBindings = &binding;

	descriptorSetLayout = vk::raii::DescriptorSetLayout(device, layoutInfo);

	// Allocate descriptor set
	vk::DescriptorSetAllocateInfo allocInfo{};
	allocInfo.descriptorPool = descriptorPool;
	allocInfo.descriptorSetCount = 1;
	vk::DescriptorSetLayout layouts[] = { *descriptorSetLayout };
	allocInfo.pSetLayouts = layouts;

	descriptorSet = std::move(device.allocateDescriptorSets(allocInfo).front());

	// Update descriptor set
	vk::DescriptorImageInfo imageInfo{};
	imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
	imageInfo.imageView = *colorImageView;
	imageInfo.sampler = *sampler;

	vk::WriteDescriptorSet writeSet{};
	writeSet.dstSet = *descriptorSet;
	writeSet.descriptorCount = 1;
	writeSet.descriptorType = vk::DescriptorType::eCombinedImageSampler;
	writeSet.pImageInfo = &imageInfo;
	writeSet.dstBinding = 0;

	device.updateDescriptorSets({ writeSet }, {});
}
