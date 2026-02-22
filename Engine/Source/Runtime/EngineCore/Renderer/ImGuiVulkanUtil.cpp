#include "ImGuiVulkanUtil.h"

#include <fstream>
#include <stdexcept>
#include <cstring>

#include "../Window.h"

static std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + filename);
    }
    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();
    return buffer;
}

ImGuiVulkanUtil::ImGuiVulkanUtil() {
}

ImGuiVulkanUtil::~ImGuiVulkanUtil() {
    cleanup();
}

void ImGuiVulkanUtil::init(vk::raii::Device& inDevice, vk::raii::PhysicalDevice& inPhysicalDevice,
                           vk::raii::Queue& inGraphicsQueue, vk::raii::CommandPool& inCommandPool, uint32_t inGraphicsQueueFamily) {
    device = &inDevice;
    physicalDevice = &inPhysicalDevice;
    graphicsQueue = &inGraphicsQueue;
    commandPool = &inCommandPool;
    graphicsQueueFamily = inGraphicsQueueFamily;
}

void ImGuiVulkanUtil::initialize(float width, float height) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    io.DisplaySize = ImVec2(width, height);
    io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);

    vulkanStyle = ImGui::GetStyle();
    vulkanStyle.Colors[ImGuiCol_TitleBg] = ImVec4(1.0f, 0.0f, 0.0f, 0.6f);
    vulkanStyle.Colors[ImGuiCol_TitleBgActive] = ImVec4(1.0f, 0.0f, 0.0f, 0.8f);
    vulkanStyle.Colors[ImGuiCol_MenuBarBg] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
    vulkanStyle.Colors[ImGuiCol_Header] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
    vulkanStyle.Colors[ImGuiCol_CheckMark] = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);

    setStyle(0);
}

void ImGuiVulkanUtil::setStyle(uint32_t index) {
    ImGuiStyle& style = ImGui::GetStyle();

    switch (index) {
        case 0:
            style = vulkanStyle;
            break;
        case 1:
            ImGui::StyleColorsClassic();
            break;
        case 2:
            ImGui::StyleColorsDark();
            break;
        case 3:
            ImGui::StyleColorsLight();
            break;
    }
}

uint32_t ImGuiVulkanUtil::findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) {
    vk::PhysicalDeviceMemoryProperties memProperties = physicalDevice->getMemoryProperties();
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("Failed to find suitable memory type!");
}

void ImGuiVulkanUtil::createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, vk::raii::Buffer& buffer, vk::raii::DeviceMemory& bufferMemory) {
    vk::BufferCreateInfo bufferInfo;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = vk::SharingMode::eExclusive;

    buffer = vk::raii::Buffer(*device, bufferInfo);

    vk::MemoryRequirements memRequirements = buffer.getMemoryRequirements();
    vk::MemoryAllocateInfo allocInfo;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    bufferMemory = vk::raii::DeviceMemory(*device, allocInfo);
    buffer.bindMemory(*bufferMemory, 0);
}

void ImGuiVulkanUtil::createImage(uint32_t width, uint32_t height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties, vk::raii::Image& image, vk::raii::DeviceMemory& imageMemory) {
    vk::ImageCreateInfo imageInfo;
    imageInfo.imageType = vk::ImageType::e2D;
    imageInfo.format = format;
    imageInfo.extent = vk::Extent3D{ width, height, 1 };
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = vk::SampleCountFlagBits::e1;
    imageInfo.tiling = tiling;
    imageInfo.usage = usage;
    imageInfo.sharingMode = vk::SharingMode::eExclusive;
    imageInfo.initialLayout = vk::ImageLayout::eUndefined;

    image = vk::raii::Image(*device, imageInfo);

    vk::MemoryRequirements memRequirements = image.getMemoryRequirements();
    vk::MemoryAllocateInfo allocInfo;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    imageMemory = vk::raii::DeviceMemory(*device, allocInfo);
    image.bindMemory(*imageMemory, 0);
}

vk::raii::ImageView ImGuiVulkanUtil::createImageView(vk::raii::Image& image, vk::Format format, vk::ImageAspectFlags aspectFlags) {
    vk::ImageViewCreateInfo viewInfo;
    viewInfo.image = image;
    viewInfo.viewType = vk::ImageViewType::e2D;
    viewInfo.format = format;
    viewInfo.subresourceRange = { aspectFlags, 0, 1, 0, 1 };

    return vk::raii::ImageView(*device, viewInfo);
}

void ImGuiVulkanUtil::transitionImageLayout(vk::Image image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout) {
    vk::CommandBufferAllocateInfo allocInfo;
    allocInfo.commandPool = *commandPool;
    allocInfo.level = vk::CommandBufferLevel::ePrimary;
    allocInfo.commandBufferCount = 1;

    vk::raii::CommandBuffer commandBuffer = std::move(device->allocateCommandBuffers(allocInfo).front());

    vk::CommandBufferBeginInfo beginInfo;
    beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

    commandBuffer.begin(beginInfo);

    vk::ImageMemoryBarrier barrier;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.image = image;
    barrier.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };

    vk::PipelineStageFlags sourceStage;
    vk::PipelineStageFlags destinationStage;

    if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal) {
        barrier.srcAccessMask = {};
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
        sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
        destinationStage = vk::PipelineStageFlagBits::eTransfer;
    } else if (oldLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
        sourceStage = vk::PipelineStageFlagBits::eTransfer;
        destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
    } else {
        throw std::invalid_argument("Unsupported layout transition!");
    }

    commandBuffer.pipelineBarrier(sourceStage, destinationStage, {}, nullptr, nullptr, barrier);

    commandBuffer.end();

    vk::SubmitInfo submitInfo;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &*commandBuffer;

    graphicsQueue->submit(submitInfo);
    graphicsQueue->waitIdle();
}

void ImGuiVulkanUtil::copyBufferToImage(vk::raii::Buffer& buffer, vk::raii::Image& image, uint32_t width, uint32_t height) {
    vk::CommandBufferAllocateInfo allocInfo;
    allocInfo.commandPool = *commandPool;
    allocInfo.level = vk::CommandBufferLevel::ePrimary;
    allocInfo.commandBufferCount = 1;

    vk::raii::CommandBuffer commandBuffer = std::move(device->allocateCommandBuffers(allocInfo).front());

    vk::CommandBufferBeginInfo beginInfo;
    beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

    commandBuffer.begin(beginInfo);

    vk::BufferImageCopy region;
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 };
    region.imageOffset = vk::Offset3D{ 0, 0, 0 };
    region.imageExtent = vk::Extent3D{ width, height, 1 };

    commandBuffer.copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, { region });

    commandBuffer.end();

    vk::SubmitInfo submitInfo;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &*commandBuffer;

    graphicsQueue->submit(submitInfo);
    graphicsQueue->waitIdle();
}

void ImGuiVulkanUtil::initResources() {
    ImGuiIO& io = ImGui::GetIO();
    unsigned char* fontData;
    int texWidth, texHeight;
    io.Fonts->GetTexDataAsRGBA32(&fontData, &texWidth, &texHeight);

    vk::DeviceSize uploadSize = texWidth * texHeight * 4 * sizeof(char);

    vk::Extent3D fontExtent{
        static_cast<uint32_t>(texWidth),
        static_cast<uint32_t>(texHeight),
        1
    };

    createImage(texWidth, texHeight, vk::Format::eR8G8B8A8Unorm,
                vk::ImageTiling::eOptimal,
                vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
                vk::MemoryPropertyFlagBits::eDeviceLocal,
                fontImage, fontImageMemory);

    fontImageView = createImageView(fontImage, vk::Format::eR8G8B8A8Unorm, vk::ImageAspectFlagBits::eColor);

    vk::raii::Buffer stagingBuffer{ nullptr };
    vk::raii::DeviceMemory stagingBufferMemory{ nullptr };
    createBuffer(uploadSize, vk::BufferUsageFlagBits::eTransferSrc,
                 vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                 stagingBuffer, stagingBufferMemory);

    void* data = stagingBufferMemory.mapMemory(0, uploadSize);
    memcpy(data, fontData, uploadSize);
    stagingBufferMemory.unmapMemory();

    transitionImageLayout(*fontImage, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
    copyBufferToImage(stagingBuffer, fontImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
    transitionImageLayout(*fontImage, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);

    vk::SamplerCreateInfo samplerInfo{};
    samplerInfo.magFilter = vk::Filter::eLinear;
    samplerInfo.minFilter = vk::Filter::eLinear;
    samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
    samplerInfo.addressModeU = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.addressModeV = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.addressModeW = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.borderColor = vk::BorderColor::eFloatOpaqueWhite;

    sampler = device->createSampler(samplerInfo);

    vk::DescriptorPoolSize poolSize{ vk::DescriptorType::eCombinedImageSampler, 1 };
    vk::DescriptorPoolCreateInfo poolInfo{};
    poolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    poolInfo.maxSets = 2;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;

    descriptorPool = device->createDescriptorPool(poolInfo);

    vk::DescriptorSetLayoutBinding binding{};
    binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
    binding.descriptorCount = 1;
    binding.stageFlags = vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eVertex;
    binding.binding = 0;

    vk::DescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &binding;

    descriptorSetLayout = device->createDescriptorSetLayout(layoutInfo);

    vk::DescriptorSetAllocateInfo allocInfo{};
    allocInfo.descriptorPool = *descriptorPool;
    allocInfo.descriptorSetCount = 1;
    vk::DescriptorSetLayout layouts[] = { *descriptorSetLayout };
    allocInfo.pSetLayouts = layouts;

    descriptorSet = std::move(device->allocateDescriptorSets(allocInfo).front());

    vk::DescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    imageInfo.imageView = *fontImageView;
    imageInfo.sampler = *sampler;

    vk::WriteDescriptorSet writeSet{};
    writeSet.dstSet = *descriptorSet;
    writeSet.descriptorCount = 1;
    writeSet.descriptorType = vk::DescriptorType::eCombinedImageSampler;
    writeSet.pImageInfo = &imageInfo;
    writeSet.dstBinding = 0;

    device->updateDescriptorSets({ writeSet }, {});

    vk::PipelineCacheCreateInfo pipelineCacheInfo{};
    pipelineCache = device->createPipelineCache(pipelineCacheInfo);

    vk::PushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = vk::ShaderStageFlagBits::eVertex;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(PushConstBlock);

    vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.setLayoutCount = 1;
    vk::DescriptorSetLayout setLayouts[] = { *descriptorSetLayout };
    pipelineLayoutInfo.pSetLayouts = setLayouts;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    pipelineLayout = device->createPipelineLayout(pipelineLayoutInfo);

    bool shaderLoadSuccess = true;
    vk::raii::ShaderModule vertexShaderModule{ nullptr };
    vk::raii::ShaderModule fragmentShaderModule{ nullptr };

    try {
        std::vector<char> vertShaderCode = readFile("../Engine/Binaries/Shaders/imgui_vertex.spv");
        std::vector<char> fragShaderCode = readFile("../Engine/Binaries/Shaders/imgui_fragment.spv");

        vk::ShaderModuleCreateInfo vertShaderModuleCreateInfo;
        vertShaderModuleCreateInfo.codeSize = vertShaderCode.size();
        vertShaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(vertShaderCode.data());
        vertexShaderModule = vk::raii::ShaderModule(*device, vertShaderModuleCreateInfo);

        vk::ShaderModuleCreateInfo fragShaderModuleCreateInfo;
        fragShaderModuleCreateInfo.codeSize = fragShaderCode.size();
        fragShaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(fragShaderCode.data());
        fragmentShaderModule = vk::raii::ShaderModule(*device, fragShaderModuleCreateInfo);
    } catch (const std::exception& e) {
        shaderLoadSuccess = false;
    }

    if (!shaderLoadSuccess) {
        return;
    }

    vk::PipelineShaderStageCreateInfo vertShaderStageInfo;
    vertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
    vertShaderStageInfo.module = *vertexShaderModule;
    vertShaderStageInfo.pName = "main";

    vk::PipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
    fragShaderStageInfo.module = *fragmentShaderModule;
    fragShaderStageInfo.pName = "main";

    vk::PipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

    vk::PipelineVertexInputStateCreateInfo vertexInputInfo;

    vk::VertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(ImDrawVert);
    bindingDescription.inputRate = vk::VertexInputRate::eVertex;

    std::array<vk::VertexInputAttributeDescription, 3> attributeDescriptions{};
    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = vk::Format::eR32G32Sfloat;
    attributeDescriptions[0].offset = offsetof(ImDrawVert, pos);

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = vk::Format::eR32G32Sfloat;
    attributeDescriptions[1].offset = offsetof(ImDrawVert, uv);

    attributeDescriptions[2].binding = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format = vk::Format::eR8G8B8A8Unorm;
    attributeDescriptions[2].offset = offsetof(ImDrawVert, col);

    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly;
    inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;
    inputAssembly.primitiveRestartEnable = vk::False;

    vk::PipelineViewportStateCreateInfo viewportState;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    vk::PipelineRasterizationStateCreateInfo rasterizer;
    rasterizer.depthClampEnable = vk::False;
    rasterizer.rasterizerDiscardEnable = vk::False;
    rasterizer.polygonMode = vk::PolygonMode::eFill;
    rasterizer.cullMode = vk::CullModeFlagBits::eNone;
    rasterizer.frontFace = vk::FrontFace::eCounterClockwise;
    rasterizer.depthBiasEnable = vk::False;
    rasterizer.lineWidth = 1.0f;

    vk::PipelineMultisampleStateCreateInfo multisampling;
    multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;
    multisampling.sampleShadingEnable = vk::False;

    vk::PipelineDepthStencilStateCreateInfo depthStencil;
    depthStencil.depthTestEnable = vk::False;
    depthStencil.depthWriteEnable = vk::False;
    depthStencil.stencilTestEnable = vk::False;

    vk::PipelineColorBlendAttachmentState colorBlendAttachment;
    colorBlendAttachment.blendEnable = vk::True;
    colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    colorBlendAttachment.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
    colorBlendAttachment.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
    colorBlendAttachment.colorBlendOp = vk::BlendOp::eAdd;
    colorBlendAttachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;
    colorBlendAttachment.dstAlphaBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
    colorBlendAttachment.alphaBlendOp = vk::BlendOp::eAdd;

    vk::PipelineColorBlendStateCreateInfo colorBlending;
    colorBlending.logicOpEnable = vk::False;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    std::vector<vk::DynamicState> dynamicStates = {
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor
    };
    vk::PipelineDynamicStateCreateInfo dynamicState;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    vk::PipelineRenderingCreateInfo pipelineRenderingCreateInfo;
    pipelineRenderingCreateInfo.colorAttachmentCount = 1;
    pipelineRenderingCreateInfo.pColorAttachmentFormats = &colorFormat;
    pipelineRenderingCreateInfo.depthAttachmentFormat = vk::Format::eUndefined;

    vk::GraphicsPipelineCreateInfo pipelineCreateInfo;
    pipelineCreateInfo.stageCount = 2;
    pipelineCreateInfo.pStages = shaderStages;
    pipelineCreateInfo.pVertexInputState = &vertexInputInfo;
    pipelineCreateInfo.pInputAssemblyState = &inputAssembly;
    pipelineCreateInfo.pViewportState = &viewportState;
    pipelineCreateInfo.pRasterizationState = &rasterizer;
    pipelineCreateInfo.pMultisampleState = &multisampling;
    pipelineCreateInfo.pColorBlendState = &colorBlending;
    pipelineCreateInfo.pDynamicState = &dynamicState;
    pipelineCreateInfo.layout = *pipelineLayout;
    pipelineCreateInfo.renderPass = nullptr;
    pipelineCreateInfo.pNext = &pipelineRenderingCreateInfo;
    pipelineCreateInfo.pDepthStencilState = &depthStencil;

    pipeline = vk::raii::Pipeline(*device, nullptr, pipelineCreateInfo);
}

bool ImGuiVulkanUtil::newFrame() {
    ImGui::NewFrame();

    ImGui::ShowDemoWindow();

    ImGui::EndFrame();
    ImGui::Render();

    ImDrawData* drawData = ImGui::GetDrawData();
    if (drawData && drawData->CmdListsCount > 0) {
        if (drawData->TotalVtxCount > vertexCount || drawData->TotalIdxCount > indexCount) {
            needsUpdateBuffers = true;
            return true;
        }
    }

    return false;
}

void ImGuiVulkanUtil::updateBuffers() {
    ImDrawData* drawData = ImGui::GetDrawData();
    if (!drawData || drawData->CmdListsCount == 0) {
        return;
    }

    vk::DeviceSize vertexBufferSize = drawData->TotalVtxCount * sizeof(ImDrawVert);
    vk::DeviceSize indexBufferSize = drawData->TotalIdxCount * sizeof(ImDrawIdx);

    if (vertexBuffer == nullptr || vertexBufferSize > maxVertexBufferSize) {
        maxVertexBufferSize = std::max(vertexBufferSize, vk::DeviceSize(1024 * 1024));
        vertexBuffer = nullptr;
        vertexBufferMemory = nullptr;
        createBuffer(maxVertexBufferSize, vk::BufferUsageFlagBits::eVertexBuffer,
                     vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                     vertexBuffer, vertexBufferMemory);
        vertexCount = drawData->TotalVtxCount;
    }

    if (indexBuffer == nullptr || indexBufferSize > maxIndexBufferSize) {
        maxIndexBufferSize = std::max(indexBufferSize, vk::DeviceSize(1024 * 1024));
        indexBuffer = nullptr;
        indexBufferMemory = nullptr;
        createBuffer(maxIndexBufferSize, vk::BufferUsageFlagBits::eIndexBuffer,
                     vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                     indexBuffer, indexBufferMemory);
        indexCount = drawData->TotalIdxCount;
    }

    ImDrawVert* vtxDst = static_cast<ImDrawVert*>(vertexBufferMemory.mapMemory(0, vertexBufferSize));
    ImDrawIdx* idxDst = static_cast<ImDrawIdx*>(indexBufferMemory.mapMemory(0, indexBufferSize));

    for (int n = 0; n < drawData->CmdListsCount; n++) {
        const ImDrawList* cmdList = drawData->CmdLists[n];
        memcpy(vtxDst, cmdList->VtxBuffer.Data, cmdList->VtxBuffer.Size * sizeof(ImDrawVert));
        memcpy(idxDst, cmdList->IdxBuffer.Data, cmdList->IdxBuffer.Size * sizeof(ImDrawIdx));
        vtxDst += cmdList->VtxBuffer.Size;
        idxDst += cmdList->IdxBuffer.Size;
    }

    vertexBufferMemory.unmapMemory();
    indexBufferMemory.unmapMemory();
}

void ImGuiVulkanUtil::drawFrame(vk::raii::CommandBuffer& commandBuffer, vk::raii::ImageView& swapChainImageView, vk::Extent2D swapChainExtent) {
    ImDrawData* drawData = ImGui::GetDrawData();
    if (!drawData || drawData->CmdListsCount == 0) {
        return;
    }

    printf("ImGui: DisplaySize=(%.0f,%.0f), CmdLists=%d\n", drawData->DisplaySize.x, drawData->DisplaySize.y, drawData->CmdListsCount);

    vk::RenderingAttachmentInfo colorAttachmentInfo;
    colorAttachmentInfo.setImageView(swapChainImageView);
    colorAttachmentInfo.setImageLayout(vk::ImageLayout::eColorAttachmentOptimal);
    colorAttachmentInfo.setLoadOp(vk::AttachmentLoadOp::eLoad);
    colorAttachmentInfo.setStoreOp(vk::AttachmentStoreOp::eStore);
    colorAttachmentInfo.setStoreOp(vk::AttachmentStoreOp::eStore);

    vk::RenderingInfo renderingInfo;
    renderingInfo.renderArea = vk::Rect2D{{0, 0}, {swapChainExtent.width, swapChainExtent.height}};
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachmentInfo;

    commandBuffer.beginRendering(renderingInfo);

    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);

    vk::Viewport viewport{};
    viewport.width = static_cast<float>(swapChainExtent.width);
    viewport.height = static_cast<float>(swapChainExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    commandBuffer.setViewport(0, viewport);

    pushConstBlock.scale = glm::vec2(2.0f / drawData->DisplaySize.x, 2.0f / drawData->DisplaySize.y);
    pushConstBlock.translate = glm::vec2(-1.0f);
    commandBuffer.pushConstants(*pipelineLayout, vk::ShaderStageFlagBits::eVertex,
                               0, vk::ArrayProxy<const PushConstBlock>(1, &pushConstBlock));

    vk::Buffer vertexBuffers[] = { *vertexBuffer };
    vk::DeviceSize offsets[] = { 0 };
    commandBuffer.bindVertexBuffers(0, vertexBuffers, offsets);
    commandBuffer.bindIndexBuffer(*indexBuffer, 0, vk::IndexType::eUint16);

    int vertexOffset = 0;
    int indexOffset = 0;

    for (int i = 0; i < drawData->CmdListsCount; i++) {
        const ImDrawList* cmdList = drawData->CmdLists[i];

        for (int j = 0; j < cmdList->CmdBuffer.Size; j++) {
            const ImDrawCmd* pcmd = &cmdList->CmdBuffer[j];

            vk::Rect2D scissor{};
            scissor.offset.x = std::max(static_cast<int32_t>(pcmd->ClipRect.x), 0);
            scissor.offset.y = std::max(static_cast<int32_t>(pcmd->ClipRect.y), 0);
            scissor.extent.width = static_cast<uint32_t>(pcmd->ClipRect.z - pcmd->ClipRect.x);
            scissor.extent.height = static_cast<uint32_t>(pcmd->ClipRect.w - pcmd->ClipRect.y);
            commandBuffer.setScissor(0, scissor);

            commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                           *pipelineLayout, 0, *descriptorSet, {});

            commandBuffer.drawIndexed(pcmd->ElemCount, 1, indexOffset, vertexOffset, 0);
            indexOffset += pcmd->ElemCount;
        }

        vertexOffset += cmdList->VtxBuffer.Size;
    }

    commandBuffer.endRendering();
}

void ImGuiVulkanUtil::handleKey(int key, int scancode, int action, int mods) {
    ImGuiIO& io = ImGui::GetIO();

    const int KEY_PRESSED = 1;
    const int KEY_RELEASED = 0;

    io.AddKeyEvent(ImGuiKey(key), action == KEY_PRESSED);

    const int KEY_LEFT_CTRL = 341;
    const int KEY_RIGHT_CTRL = 345;
    const int KEY_LEFT_SHIFT = 340;
    const int KEY_RIGHT_SHIFT = 344;
    const int KEY_LEFT_ALT = 342;
    const int KEY_RIGHT_ALT = 346;
    const int KEY_LEFT_SUPER = 343;
    const int KEY_RIGHT_SUPER = 347;

    io.KeyCtrl = ImGui::IsKeyDown(ImGuiKey_LeftCtrl) || ImGui::IsKeyDown(ImGuiKey_RightCtrl);
    io.KeyShift = ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_RightShift);
    io.KeyAlt = ImGui::IsKeyDown(ImGuiKey_LeftAlt) || ImGui::IsKeyDown(ImGuiKey_RightAlt);
    io.KeySuper = ImGui::IsKeyDown(ImGuiKey_LeftSuper) || ImGui::IsKeyDown(ImGuiKey_RightSuper);
}

bool ImGuiVulkanUtil::getWantKeyCapture() {
    return ImGui::GetIO().WantCaptureKeyboard;
}

void ImGuiVulkanUtil::charPressed(uint32_t key) {
    ImGuiIO& io = ImGui::GetIO();
    io.AddInputCharacter(key);
}

void ImGuiVulkanUtil::handleMouseButton(int button, int action, int mods) {
    ImGuiIO& io = ImGui::GetIO();
    if (button >= 0 && button < ImGuiMouseButton_COUNT) {
        io.MouseDown[button] = (action == 1);
    }
}

void ImGuiVulkanUtil::handleMousePosition(double xpos, double ypos) {
    ImGuiIO& io = ImGui::GetIO();
    io.MousePos = ImVec2(static_cast<float>(xpos), static_cast<float>(ypos));
}

void ImGuiVulkanUtil::handleMouseScroll(double xoffset, double yoffset) {
    ImGuiIO& io = ImGui::GetIO();
    io.MouseWheelH += static_cast<float>(xoffset);
    io.MouseWheel += static_cast<float>(yoffset);
}

void ImGuiVulkanUtil::cleanup() {
    if (device) {
        device->waitIdle();
    }

    fontImage = nullptr;
    fontImageView = nullptr;
    sampler = nullptr;
    vertexBuffer = nullptr;
    indexBuffer = nullptr;
    vertexBufferMemory = nullptr;
    indexBufferMemory = nullptr;
    fontImageMemory = nullptr;
    pipeline = nullptr;
    pipelineLayout = nullptr;
    pipelineCache = nullptr;
    descriptorPool = nullptr;
    descriptorSetLayout = nullptr;
    descriptorSet = nullptr;

    ImGui::DestroyContext();
}
