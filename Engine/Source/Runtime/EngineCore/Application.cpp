#define STB_IMAGE_IMPLEMENTATION
#include "Runtime/EngineCore/Application.h"
#include <chrono>

//TODO: Will probably move this to a window class in the future
constexpr uint32_t WIDTH = 800;
constexpr uint32_t HEIGHT = 600;

//TODO: Will move to vulkan specific RHI types
const std::vector<char const*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

constexpr int      MAX_FRAMES_IN_FLIGHT = 2;

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

Application::Application()
{
}

Application::~Application()
{
}

void Application::Run()
{
    InitializeWindow();
    InitializeVulkan();
    MainLoop();
    Cleanup();
}

void Application::InitializeWindow()
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    Window = glfwCreateWindow(WIDTH, HEIGHT, "RealityEngine", nullptr, nullptr);
    glfwSetWindowUserPointer(Window, this);
    glfwSetFramebufferSizeCallback(Window, framebufferResizeCallback);
}

void Application::InitializeVulkan()
{
    CreateInstance();
    SetupDebugMessenger();
    CreateSurface();
    PickPhysicalDevice();
    CreateLogicalDevice();
    CreateSwapChain();
    CreateImageViews();
    CreateDescriptorSetLayout();
    CreateGraphicsPipeline();
    CreateCommandPool();
    CreateDepthResources();
    CreateTextureImage();
    CreateTextureImageView();
    CreateTextureSampler();
    CreateVertexBuffer();
    CreateIndexBuffer();
    CreateUniformBuffers();
    CreateDescriptorPool();
    CreateDescriptorSets();
    CreateCommandBuffers();
    CreateSyncObjects();
}

void Application::CreateInstance()
{
    vk::ApplicationInfo appInfo;
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine",
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = vk::ApiVersion14;

    // Get the required layers
    std::vector<char const*> requiredLayers;
    if (enableValidationLayers)
    {
        requiredLayers.assign(validationLayers.begin(), validationLayers.end());
    }

    // Check if the required layers are supported by the Vulkan implementation.
    auto layerProperties = VulkanContext.enumerateInstanceLayerProperties();
    for (auto const& requiredLayer : requiredLayers)
    {
        if (std::ranges::none_of(layerProperties,
            [requiredLayer](auto const& layerProperty) { return strcmp(layerProperty.layerName, requiredLayer) == 0; }))
        {
            throw std::runtime_error("Required layer not supported: " + std::string(requiredLayer));
        }
    }
    // Get the required extensions.
    auto requiredExtensions = getRequiredExtensions();

    // Check if the required extensions are supported by the Vulkan implementation.
    auto extensionProperties = VulkanContext.enumerateInstanceExtensionProperties();
    for (auto const& requiredExtension : requiredExtensions)
    {
        if (std::ranges::none_of(extensionProperties,
            [requiredExtension](auto const& extensionProperty) { return strcmp(extensionProperty.extensionName, requiredExtension) == 0; }))
        {
            throw std::runtime_error("Required extension not supported: " + std::string(requiredExtension));
        }
    }


    vk::InstanceCreateInfo createInfo;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledLayerCount = static_cast<uint32_t>(requiredLayers.size());
    createInfo.ppEnabledLayerNames = requiredLayers.data();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size());
    createInfo.ppEnabledExtensionNames = requiredExtensions.data();
	
	VulkanInstance = vk::raii::Instance(VulkanContext, createInfo);
}

void Application::SetupDebugMessenger()
{
    if (!enableValidationLayers)
    {
        return;
    }

    vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
    vk::DebugUtilsMessageTypeFlagsEXT    messageTypeFlags(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);
    vk::DebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfoEXT;
    debugUtilsMessengerCreateInfoEXT.messageSeverity = severityFlags;
	debugUtilsMessengerCreateInfoEXT.messageType = messageTypeFlags;
	debugUtilsMessengerCreateInfoEXT.pfnUserCallback = &debugCallback;
    
    VulkanDebugMessenger = VulkanInstance.createDebugUtilsMessengerEXT(debugUtilsMessengerCreateInfoEXT);
}

void Application::CreateSurface()
{
    VkSurfaceKHR _surface;
    if (glfwCreateWindowSurface(*VulkanInstance, Window, nullptr, &_surface) != 0) {
        throw std::runtime_error("failed to create window surface!");
    }
    VulkanSurface = vk::raii::SurfaceKHR(VulkanInstance, _surface);
}

void Application::PickPhysicalDevice()
{
    std::vector<vk::raii::PhysicalDevice> devices = VulkanInstance.enumeratePhysicalDevices();
    const auto                            devIter = std::ranges::find_if(
        devices,
        [&](auto const& device) {
            // Check if the device supports the Vulkan 1.3 API version
            bool supportsVulkan1_3 = device.getProperties().apiVersion >= VK_API_VERSION_1_3;

            // Check if any of the queue families support graphics operations
            auto queueFamilies = device.getQueueFamilyProperties();
            bool supportsGraphics =
                std::ranges::any_of(queueFamilies, [](auto const& qfp) { return !!(qfp.queueFlags & vk::QueueFlagBits::eGraphics); });

            // Check if all required device extensions are available
            auto availableDeviceExtensions = device.enumerateDeviceExtensionProperties();
            bool supportsAllRequiredExtensions =
                std::ranges::all_of(VulkanRequiredDeviceExtension,
                    [&availableDeviceExtensions](auto const& requiredDeviceExtension) {
                        return std::ranges::any_of(availableDeviceExtensions,
                            [requiredDeviceExtension](auto const& availableDeviceExtension) { return strcmp(availableDeviceExtension.extensionName, requiredDeviceExtension) == 0; });
                    });

            auto features = device.template getFeatures2<vk::PhysicalDeviceFeatures2,
                vk::PhysicalDeviceVulkan11Features,
                vk::PhysicalDeviceVulkan13Features,
                vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
            bool supportsRequiredFeatures = features.template get<vk::PhysicalDeviceVulkan11Features>().shaderDrawParameters && features.template get<vk::PhysicalDeviceVulkan13Features>().synchronization2 && 
                features.template get<vk::PhysicalDeviceFeatures2>().features.samplerAnisotropy &&
                features.template get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
                features.template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState;

            return supportsVulkan1_3 && supportsGraphics && supportsAllRequiredExtensions && supportsRequiredFeatures;
        });
    if (devIter != devices.end())
    {
        VulkanPhysicalDevice = *devIter;
    }
    else
    {
        throw std::runtime_error("failed to find a suitable GPU!");
    }
}

void Application::CreateLogicalDevice()
{
    // find the index of the first queue family that supports graphics
    std::vector<vk::QueueFamilyProperties> queueFamilyProperties = VulkanPhysicalDevice.getQueueFamilyProperties();

    // get the first index into queueFamilyProperties which supports both graphics and present
    for (uint32_t qfpIndex = 0; qfpIndex < queueFamilyProperties.size(); qfpIndex++)
    {
        if ((queueFamilyProperties[qfpIndex].queueFlags & vk::QueueFlagBits::eGraphics) &&
            VulkanPhysicalDevice.getSurfaceSupportKHR(qfpIndex, *VulkanSurface))
        {
            // found a queue family that supports both graphics and present
            queueIndex = qfpIndex;
            break;
        }
    }
    if (queueIndex == ~0)
    {
        throw std::runtime_error("Could not find a queue for graphics and present -> terminating");
    }

    vk::PhysicalDeviceFeatures2 feature2;
    feature2.features.samplerAnisotropy = true;
    // query for Vulkan 1.3 features
    vk::PhysicalDeviceVulkan13Features vulkan13Features{};
    vulkan13Features.dynamicRendering = true;
    vulkan13Features.synchronization2 = true;

    vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT extendedDynamicStateFeatures{};
    extendedDynamicStateFeatures.extendedDynamicState = true;

	vk::PhysicalDeviceVulkan11Features vulkan11Features{};
    vulkan11Features.shaderDrawParameters = true;

    vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan11Features, vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT> featureChain(
        feature2,                                   // vk::PhysicalDeviceFeatures2
        vulkan11Features,                       // vk::PhysicalDeviceVulkan11Features
        vulkan13Features,                     // vk::PhysicalDeviceVulkan13Features
        extendedDynamicStateFeatures          // vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT
    );

    // create a Device
    float                     queuePriority = 0.5f;
    vk::DeviceQueueCreateInfo deviceQueueCreateInfo;
    deviceQueueCreateInfo.queueFamilyIndex = queueIndex;
    deviceQueueCreateInfo.queueCount = 1;
    deviceQueueCreateInfo.pQueuePriorities = &queuePriority;

    vk::DeviceCreateInfo deviceCreateInfo;
    deviceCreateInfo.pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>();
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = &deviceQueueCreateInfo;
    deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(VulkanRequiredDeviceExtension.size());
    deviceCreateInfo.ppEnabledExtensionNames = VulkanRequiredDeviceExtension.data();

    VulkanLogicalDevice = vk::raii::Device(VulkanPhysicalDevice, deviceCreateInfo);
    VulkanGraphicsQueue = vk::raii::Queue(VulkanLogicalDevice, queueIndex, 0);
}

void Application::CreateSwapChain()
{
    auto surfaceCapabilities = VulkanPhysicalDevice.getSurfaceCapabilitiesKHR(*VulkanSurface);
    VulkanSwapChainExtent = chooseSwapExtent(surfaceCapabilities);
    VulkanSwapChainSurfaceFormat = chooseSwapSurfaceFormat(VulkanPhysicalDevice.getSurfaceFormatsKHR(*VulkanSurface));

    vk::SwapchainCreateInfoKHR swapChainCreateInfo;
    swapChainCreateInfo.surface = *VulkanSurface;
    swapChainCreateInfo.minImageCount = chooseSwapMinImageCount(surfaceCapabilities);
    swapChainCreateInfo.imageFormat = VulkanSwapChainSurfaceFormat.format;
    swapChainCreateInfo.imageColorSpace = VulkanSwapChainSurfaceFormat.colorSpace;
    swapChainCreateInfo.imageExtent = VulkanSwapChainExtent;
    swapChainCreateInfo.imageArrayLayers = 1;
    swapChainCreateInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
    swapChainCreateInfo.imageSharingMode = vk::SharingMode::eExclusive;
    swapChainCreateInfo.preTransform = surfaceCapabilities.currentTransform;
    swapChainCreateInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    swapChainCreateInfo.presentMode = chooseSwapPresentMode(VulkanPhysicalDevice.getSurfacePresentModesKHR(*VulkanSurface));
	swapChainCreateInfo.clipped = true;

    VulkanSwapChain = vk::raii::SwapchainKHR(VulkanLogicalDevice, swapChainCreateInfo);
    VulkanSwapChainImages = VulkanSwapChain.getImages();
}

vk::raii::ImageView Application::CreateImageView(vk::raii::Image& image, vk::Format format, vk::ImageAspectFlags aspectFlags)
{
    vk::ImageViewCreateInfo viewInfo;
    viewInfo.image = image;
    viewInfo.viewType = vk::ImageViewType::e2D;
    viewInfo.format = format;
    viewInfo.subresourceRange = { aspectFlags, 0, 1, 0, 1 };

    return vk::raii::ImageView(VulkanLogicalDevice, viewInfo);
}

void Application::CreateImageViews()
{
    assert(VulkanSwapChainImageViews.empty());

    vk::ImageViewCreateInfo imageViewCreateInfo;
    imageViewCreateInfo.viewType = vk::ImageViewType::e2D;
    imageViewCreateInfo.format = VulkanSwapChainSurfaceFormat.format;
	imageViewCreateInfo.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };

    for (auto& image : VulkanSwapChainImages)
    {
        imageViewCreateInfo.image = image;
        VulkanSwapChainImageViews.emplace_back(VulkanLogicalDevice, imageViewCreateInfo);
    }
}

void Application::CreateDescriptorSetLayout()
{
    //Create Uniform Buffer Object Layout Binding
    std::array bindings = {
      vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex, nullptr),
      vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, nullptr)
    };

    vk::DescriptorSetLayoutCreateInfo layoutInfo({}, bindings.size(), bindings.data());

	VulkanDescriptorSetLayout = vk::raii::DescriptorSetLayout(VulkanLogicalDevice, layoutInfo);
}

void Application::CreateGraphicsPipeline()
{
    vk::raii::ShaderModule VertexShaderModule = CreateShaderModule(ReadFile("../Engine/Binaries/Shaders/ShaderType_Vertex.spv"));
    vk::raii::ShaderModule FragmentShaderModule = CreateShaderModule(ReadFile("../Engine/Binaries/Shaders/ShaderTypes_Fragment.spv"));

    vk::PipelineShaderStageCreateInfo VertShaderStageInfo;
    VertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
    VertShaderStageInfo.module = VertexShaderModule;
    VertShaderStageInfo.pName = "main";

    vk::PipelineShaderStageCreateInfo FragShaderStageInfo{};
    FragShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
    FragShaderStageInfo.module = FragmentShaderModule;
    FragShaderStageInfo.pName = "main";

    vk::PipelineShaderStageCreateInfo ShaderStages[] = { VertShaderStageInfo, FragShaderStageInfo };

    // Graphic Pipeline
    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();

    vk::PipelineVertexInputStateCreateInfo   vertexInputInfo;//<--- InputAssembly
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

    vk::PipelineRasterizationStateCreateInfo rasterizer;//<--- Rasterizer Stages
    rasterizer.depthClampEnable = vk::False;
    rasterizer.rasterizerDiscardEnable = vk::False;
    rasterizer.polygonMode = vk::PolygonMode::eFill;
    rasterizer.cullMode = vk::CullModeFlagBits::eBack;
    rasterizer.frontFace = vk::FrontFace::eCounterClockwise;
    rasterizer.depthBiasEnable = vk::False;
    rasterizer.lineWidth = 1.0f;

    vk::PipelineMultisampleStateCreateInfo multisampling;
    multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;
    multisampling.sampleShadingEnable = vk::False;

    vk::PipelineDepthStencilStateCreateInfo depthStencil;
    depthStencil.depthTestEnable = vk::True;
    depthStencil.depthWriteEnable = vk::True;
    depthStencil.depthCompareOp = vk::CompareOp::eLess;
    depthStencil.depthBoundsTestEnable = vk::False;
    depthStencil.stencilTestEnable = vk::False;

    vk::PipelineColorBlendAttachmentState colorBlendAttachment;
    colorBlendAttachment.blendEnable = vk::False;
    colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

    vk::PipelineColorBlendStateCreateInfo colorBlending;
    colorBlending.logicOpEnable = vk::False;
    colorBlending.logicOp = vk::LogicOp::eCopy;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    std::vector dynamicStates = {
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor };
    vk::PipelineDynamicStateCreateInfo dynamicState;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    vk::PipelineLayoutCreateInfo pipelineLayoutInfo;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &*VulkanDescriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 0;

    VulkanPipelineLayout = vk::raii::PipelineLayout(VulkanLogicalDevice, pipelineLayoutInfo);

    vk::Format depthFormat = findDepthFormat();

	vk::PipelineRenderingCreateInfo PipelineRenderingCreateInfo;
    PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    PipelineRenderingCreateInfo.pColorAttachmentFormats = &VulkanSwapChainSurfaceFormat.format;
    PipelineRenderingCreateInfo.depthAttachmentFormat = depthFormat;

    vk::GraphicsPipelineCreateInfo GraphicPipelineCreateInfo;
    GraphicPipelineCreateInfo.stageCount = 2;
    GraphicPipelineCreateInfo.pStages = ShaderStages;
    GraphicPipelineCreateInfo.pVertexInputState = &vertexInputInfo;
    GraphicPipelineCreateInfo.pInputAssemblyState = &inputAssembly;
    GraphicPipelineCreateInfo.pViewportState = &viewportState;
    GraphicPipelineCreateInfo.pRasterizationState = &rasterizer;
    GraphicPipelineCreateInfo.pMultisampleState = &multisampling;
    GraphicPipelineCreateInfo.pColorBlendState = &colorBlending;
    GraphicPipelineCreateInfo.pDynamicState = &dynamicState;
    GraphicPipelineCreateInfo.layout = VulkanPipelineLayout;
    GraphicPipelineCreateInfo.renderPass = nullptr; // if we pass something, we dont use dynamic rendering and we have to use traditional renderpass rendering
    GraphicPipelineCreateInfo.pNext = &PipelineRenderingCreateInfo; // Link the pipeline rendering info
    GraphicPipelineCreateInfo.pDepthStencilState = &depthStencil;

    VulkanGraphicsPipeline = vk::raii::Pipeline(VulkanLogicalDevice, nullptr, GraphicPipelineCreateInfo);
}

void Application::CreateCommandPool()
{
    vk::CommandPoolCreateInfo poolInfo;
    poolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    poolInfo.queueFamilyIndex = queueIndex;
    
    VulkanCommandPool = vk::raii::CommandPool(VulkanLogicalDevice, poolInfo);
}

void Application::CreateDepthResources()
{
    vk::Format depthFormat = findDepthFormat();
    CreateImage(VulkanSwapChainExtent.width, VulkanSwapChainExtent.height, depthFormat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::MemoryPropertyFlagBits::eDeviceLocal, depthImage, depthImageMemory);
    depthImageView = CreateImageView(depthImage, depthFormat, vk::ImageAspectFlagBits::eDepth);
}

void Application::CreateTextureImage()
{
    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load("E:/Quasar/Engine/Content/Textures/statue.jpg", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    vk::DeviceSize imageSize = texWidth * texHeight * 4;

    if (!pixels) {
        throw std::runtime_error("failed to load texture image!");
    }

    vk::raii::Buffer stagingBuffer({});
    vk::raii::DeviceMemory stagingBufferMemory({});

    CreateBuffer(imageSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBuffer, stagingBufferMemory);

    void* data = stagingBufferMemory.mapMemory(0, imageSize);
    memcpy(data, pixels, imageSize);
    stagingBufferMemory.unmapMemory();

    stbi_image_free(pixels);

    CreateImage(texWidth, texHeight, vk::Format::eR8G8B8A8Srgb, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled, vk::MemoryPropertyFlagBits::eDeviceLocal, textureImage, textureImageMemory);

    transitionImageLayout(textureImage, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
    copyBufferToImage(stagingBuffer, textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
    transitionImageLayout(textureImage, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);

}

void Application::CreateImage(uint32_t width, uint32_t height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties, vk::raii::Image& image, vk::raii::DeviceMemory& imageMemory)
{
    vk::ImageCreateInfo imageInfo;
    imageInfo.imageType = vk::ImageType::e2D;
    imageInfo.format = format,
	imageInfo.extent = vk::Extent3D{ width, height, 1 };
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1,
	imageInfo.samples = vk::SampleCountFlagBits::e1; 
	imageInfo.tiling = tiling;
    imageInfo.usage = usage;
    imageInfo.sharingMode = vk::SharingMode::eExclusive;
    imageInfo.initialLayout = vk::ImageLayout::eUndefined;

    image = vk::raii::Image(VulkanLogicalDevice, imageInfo);

    vk::MemoryRequirements memRequirements = image.getMemoryRequirements();
    vk::MemoryAllocateInfo allocInfo;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    imageMemory = vk::raii::DeviceMemory(VulkanLogicalDevice, allocInfo);
    image.bindMemory(imageMemory, 0);
}

void Application::CreateTextureImageView()
{
    textureImageView = CreateImageView(textureImage, vk::Format::eR8G8B8A8Srgb, vk::ImageAspectFlagBits::eColor);
}

void Application::CreateTextureSampler()
{
    vk::PhysicalDeviceProperties properties = VulkanPhysicalDevice.getProperties();
    vk::SamplerCreateInfo samplerInfo;
    samplerInfo.magFilter = vk::Filter::eLinear;
    samplerInfo.minFilter = vk::Filter::eLinear;
    samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
    samplerInfo.addressModeU = vk::SamplerAddressMode::eRepeat;
    samplerInfo.addressModeV = vk::SamplerAddressMode::eRepeat;
    samplerInfo.addressModeW = vk::SamplerAddressMode::eRepeat;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.anisotropyEnable = vk::True;
    samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
    samplerInfo.compareEnable = vk::False;
    samplerInfo.compareOp = vk::CompareOp::eAlways;

    textureSampler = vk::raii::Sampler(VulkanLogicalDevice, samplerInfo);
}

void Application::CreateBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, vk::raii::Buffer& buffer, vk::raii::DeviceMemory& bufferMemory)
{
    vk::BufferCreateInfo bufferInfo;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = vk::SharingMode::eExclusive;

    buffer = vk::raii::Buffer(VulkanLogicalDevice, bufferInfo);

    vk::MemoryRequirements memRequirements = buffer.getMemoryRequirements();
    vk::MemoryAllocateInfo allocInfo;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    bufferMemory = vk::raii::DeviceMemory(VulkanLogicalDevice, allocInfo);
    buffer.bindMemory(*bufferMemory, 0);
}

void Application::CreateDescriptorPool()
{
    std::array poolSize{
     vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, MAX_FRAMES_IN_FLIGHT),
     vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, MAX_FRAMES_IN_FLIGHT)
    };
    vk::DescriptorPoolCreateInfo poolInfo;
    poolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    poolInfo.maxSets = MAX_FRAMES_IN_FLIGHT;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSize.size());
    poolInfo.pPoolSizes = poolSize.data();

    VulkanDescriptorPool = vk::raii::DescriptorPool(VulkanLogicalDevice, poolInfo);
}

void Application::CreateDescriptorSets()
{
    std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, *VulkanDescriptorSetLayout);
    vk::DescriptorSetAllocateInfo allocInfo;
    allocInfo.descriptorPool = VulkanDescriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
    allocInfo.pSetLayouts = layouts.data();

    VulkanDescriptorSets.clear();
    VulkanDescriptorSets = VulkanLogicalDevice.allocateDescriptorSets(allocInfo);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        vk::DescriptorBufferInfo bufferInfo;
        bufferInfo.buffer = VulkanUniformBuffers[i];
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformBufferObject);

        vk::DescriptorImageInfo imageInfo;
        imageInfo.sampler = textureSampler;
        imageInfo.imageView = textureImageView;
        imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        
        vk::WriteDescriptorSet bufferdescriptorWrite;
        bufferdescriptorWrite.dstSet = VulkanDescriptorSets[i];
        bufferdescriptorWrite.dstBinding = 0;
        bufferdescriptorWrite.dstArrayElement = 0;
        bufferdescriptorWrite.descriptorCount = 1;
        bufferdescriptorWrite.descriptorType = vk::DescriptorType::eUniformBuffer;
        bufferdescriptorWrite.pBufferInfo = &bufferInfo;

        vk::WriteDescriptorSet imageinfodescriptorWrite;
        imageinfodescriptorWrite.dstSet = VulkanDescriptorSets[i];
        imageinfodescriptorWrite.dstBinding = 1;
        imageinfodescriptorWrite.dstArrayElement = 0;
        imageinfodescriptorWrite.descriptorCount = 1;
        imageinfodescriptorWrite.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        imageinfodescriptorWrite.pImageInfo = &imageInfo;
        
        std::array descriptorWrites{ bufferdescriptorWrite , imageinfodescriptorWrite };
        VulkanLogicalDevice.updateDescriptorSets(descriptorWrites, {});
    }
}

void Application::CreateVertexBuffer()
{
    vk::DeviceSize         bufferSize = sizeof(vertices[0]) * vertices.size();
    vk::raii::Buffer       stagingBuffer({});
    vk::raii::DeviceMemory stagingBufferMemory({});
    CreateBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBuffer, stagingBufferMemory);

    void* dataStaging = stagingBufferMemory.mapMemory(0, bufferSize);
    memcpy(dataStaging, vertices.data(), bufferSize);
    stagingBufferMemory.unmapMemory();

    CreateBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal, VulkanVertexBuffer, VulkanVertexBufferMemory);

    copyBuffer(stagingBuffer, VulkanVertexBuffer, bufferSize);
}

void Application::CreateIndexBuffer()
{
    vk::DeviceSize bufferSize = sizeof(indices[0]) * indices.size();

    vk::raii::Buffer       stagingBuffer({});
    vk::raii::DeviceMemory stagingBufferMemory({});
    CreateBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBuffer, stagingBufferMemory);

    void* data = stagingBufferMemory.mapMemory(0, bufferSize);
    memcpy(data, indices.data(), (size_t)bufferSize);
    stagingBufferMemory.unmapMemory();

    CreateBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal, VulkanIndexBuffer, VulkanIndexBufferMemory);

    copyBuffer(stagingBuffer, VulkanIndexBuffer, bufferSize);
}

void Application::CreateUniformBuffers()
{
    VulkanUniformBuffers.clear();
    VulkanUniformBuffersMemory.clear();
    VulkanUniformBuffersMapped.clear();

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vk::DeviceSize bufferSize = sizeof(UniformBufferObject);
        vk::raii::Buffer buffer({});
        vk::raii::DeviceMemory bufferMem({});
        CreateBuffer(bufferSize, vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, buffer, bufferMem);
        VulkanUniformBuffers.emplace_back(std::move(buffer));
        VulkanUniformBuffersMemory.emplace_back(std::move(bufferMem));
        VulkanUniformBuffersMapped.emplace_back(VulkanUniformBuffersMemory[i].mapMemory(0, bufferSize));
    }
}

void Application::CreateCommandBuffers()
{
    VulkanCommandBuffers.clear();

    vk::CommandBufferAllocateInfo allocInfo;
    allocInfo.commandPool = VulkanCommandPool;
    allocInfo.level = vk::CommandBufferLevel::ePrimary;
    allocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;

	VulkanCommandBuffers = vk::raii::CommandBuffers(VulkanLogicalDevice, allocInfo);
}

void Application::CreateSyncObjects()
{
    assert(VulkanPresentCompleteSemaphores.empty() && VulkanRenderFinishedSemaphores.empty() && inFlightFences.empty());

    for (size_t i = 0; i < VulkanSwapChainImages.size(); i++)
    {
        VulkanRenderFinishedSemaphores.emplace_back(VulkanLogicalDevice, vk::SemaphoreCreateInfo());
    }

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        vk::FenceCreateInfo FenceCreateInfo;
        FenceCreateInfo.flags = vk::FenceCreateFlagBits::eSignaled;

        VulkanPresentCompleteSemaphores.emplace_back(VulkanLogicalDevice, vk::SemaphoreCreateInfo());
        inFlightFences.emplace_back(VulkanLogicalDevice, FenceCreateInfo);
    }
};

void Application::UpdateUniformBuffer(uint32_t currentImage)
{
    static auto startTime = std::chrono::high_resolution_clock::now();

    auto  currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float>(currentTime - startTime).count();

    UniformBufferObject ubo{};
    ubo.model = rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.view = lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.proj = glm::perspective(glm::radians(45.0f), static_cast<float>(VulkanSwapChainExtent.width) / static_cast<float>(VulkanSwapChainExtent.height), 0.1f, 10.0f);
    ubo.proj[1][1] *= -1;

    memcpy(VulkanUniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
}

void Application::drawFrame()
{
    // Note: inFlightFences, presentCompleteSemaphores, and commandBuffers are indexed by frameIndex,
            //       while renderFinishedSemaphores is indexed by imageIndex
    auto fenceResult = VulkanLogicalDevice.waitForFences(*inFlightFences[frameIndex], vk::True, UINT64_MAX);
    if (fenceResult != vk::Result::eSuccess)
    {
        throw std::runtime_error("failed to wait for fence!");
    }

    auto [result, imageIndex] = VulkanSwapChain.acquireNextImage(UINT64_MAX, *VulkanPresentCompleteSemaphores[frameIndex], nullptr);

    // Due to VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS being defined, eErrorOutOfDateKHR can be checked as a result
    // here and does not need to be caught by an exception.
    if (result == vk::Result::eErrorOutOfDateKHR)
    {
        RecreateSwapChain();
        return;
    }
    // On other success codes than eSuccess and eSuboptimalKHR we just throw an exception.
    // On any error code, aquireNextImage already threw an exception.
    if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR)
    {
        assert(result == vk::Result::eTimeout || result == vk::Result::eNotReady);
        throw std::runtime_error("failed to acquire swap chain image!");
    }
    UpdateUniformBuffer(frameIndex);

    // Only reset the fence if we are submitting work
    VulkanLogicalDevice.resetFences(*inFlightFences[frameIndex]);

    VulkanCommandBuffers[frameIndex].reset();
    recordCommandBuffer(imageIndex);

    vk::PipelineStageFlags waitDestinationStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
	vk::SubmitInfo   submitInfo;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &*VulkanPresentCompleteSemaphores[frameIndex];
    submitInfo.pWaitDstStageMask = &waitDestinationStageMask;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &*VulkanCommandBuffers[frameIndex];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &*VulkanRenderFinishedSemaphores[imageIndex];

    VulkanGraphicsQueue.submit(submitInfo, *inFlightFences[frameIndex]);

	vk::PresentInfoKHR presentInfoKHR;
    presentInfoKHR.waitSemaphoreCount = 1;
    presentInfoKHR.pWaitSemaphores = &*VulkanRenderFinishedSemaphores[imageIndex];
    presentInfoKHR.swapchainCount = 1;
    presentInfoKHR.pSwapchains = &*VulkanSwapChain;
    presentInfoKHR.pImageIndices = &imageIndex;

    result = VulkanGraphicsQueue.presentKHR(presentInfoKHR);
    // Due to VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS being defined, eErrorOutOfDateKHR can be checked as a result
    // here and does not need to be caught by an exception.
    if ((result == vk::Result::eSuboptimalKHR) || (result == vk::Result::eErrorOutOfDateKHR) || framebufferResized)
    {
        framebufferResized = false;
        RecreateSwapChain();
    }
    else
    {
        // There are no other success codes than eSuccess; on any error code, presentKHR already threw an exception.
        assert(result == vk::Result::eSuccess);
    }
    frameIndex = (frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Application::recordCommandBuffer(uint32_t imageIndex)
{
    auto& commandBuffer = VulkanCommandBuffers[frameIndex];
    commandBuffer.begin({});
    // Before starting rendering, transition the swapchain image to COLOR_ATTACHMENT_OPTIMAL
    transition_image_layout(
        VulkanSwapChainImages[imageIndex],
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eColorAttachmentOptimal,
        {},                                                        // srcAccessMask (no need to wait for previous operations)
        vk::AccessFlagBits2::eColorAttachmentWrite,                // dstAccessMask
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,        // srcStage
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,        // dstStage
        vk::ImageAspectFlagBits::eColor);
    // Transition depth image to depth attachment optimal layout
    transition_image_layout(
        *depthImage,
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eDepthAttachmentOptimal,
        vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
        vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
        vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
        vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
        vk::ImageAspectFlagBits::eDepth);

    vk::ClearValue clearColor = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
    vk::ClearValue clearDepth = vk::ClearDepthStencilValue(1.0f, 0);

    vk::RenderingAttachmentInfo colorAttachmentInfo;
    colorAttachmentInfo.setImageView(VulkanSwapChainImageViews[imageIndex]);
    colorAttachmentInfo.setImageLayout(vk::ImageLayout::eColorAttachmentOptimal);
    colorAttachmentInfo.setLoadOp(vk::AttachmentLoadOp::eClear);
    colorAttachmentInfo.setStoreOp(vk::AttachmentStoreOp::eStore);
    colorAttachmentInfo.setClearValue(clearColor);

    vk::RenderingAttachmentInfo depthAttachmentInfo;
    depthAttachmentInfo.setImageView(depthImageView);
    depthAttachmentInfo.setImageLayout(vk::ImageLayout::eDepthAttachmentOptimal);
    depthAttachmentInfo.setLoadOp(vk::AttachmentLoadOp::eClear);
    depthAttachmentInfo.setStoreOp(vk::AttachmentStoreOp::eDontCare);
    depthAttachmentInfo.setClearValue(clearDepth);

    vk::RenderingInfo renderingInfo;

    vk::Rect2D rect2d;
    rect2d.offset = vk::Offset2D{ 0, 0 };
    rect2d.extent = VulkanSwapChainExtent;

    renderingInfo.renderArea = rect2d;
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachmentInfo;
    renderingInfo.pDepthAttachment = &depthAttachmentInfo;

    commandBuffer.beginRendering(renderingInfo);
    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *VulkanGraphicsPipeline);
    commandBuffer.setViewport(0, vk::Viewport(0.0f, 0.0f, static_cast<float>(VulkanSwapChainExtent.width), static_cast<float>(VulkanSwapChainExtent.height), 0.0f, 1.0f));
    commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), VulkanSwapChainExtent));
    commandBuffer.bindVertexBuffers(0, *VulkanVertexBuffer, { 0 });
    commandBuffer.bindIndexBuffer(*VulkanIndexBuffer, 0, vk::IndexType::eUint16);
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, VulkanPipelineLayout, 0, *VulkanDescriptorSets[frameIndex], nullptr);
    commandBuffer.drawIndexed(indices.size(), 1, 0, 0, 0);
    commandBuffer.endRendering();
    // After rendering, transition the swapchain image to PRESENT_SRC
    transition_image_layout(
        VulkanSwapChainImages[imageIndex],
        vk::ImageLayout::eColorAttachmentOptimal,
        vk::ImageLayout::ePresentSrcKHR,
        vk::AccessFlagBits2::eColorAttachmentWrite,                // srcAccessMask
        {},                                                        // dstAccessMask
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,        // srcStage
        vk::PipelineStageFlagBits2::eBottomOfPipe,                 // dstStage
        vk::ImageAspectFlagBits::eColor);
    commandBuffer.end();
}

void Application::transition_image_layout(
    vk::Image               image,
    vk::ImageLayout         old_layout,
    vk::ImageLayout         new_layout,
    vk::AccessFlags2        src_access_mask,
    vk::AccessFlags2        dst_access_mask,
    vk::PipelineStageFlags2 src_stage_mask,
    vk::PipelineStageFlags2 dst_stage_mask,
    vk::ImageAspectFlags    image_aspect_flags)
{
    vk::ImageMemoryBarrier2 barrier;
    barrier.srcStageMask = src_stage_mask;
    barrier.srcAccessMask = src_access_mask;
    barrier.dstStageMask = dst_stage_mask;
    barrier.dstAccessMask = dst_access_mask;
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange = {
                image_aspect_flags,
                 0,
                1,
                0,
                1 };

    vk::DependencyInfo dependency_info;
    dependency_info.dependencyFlags = {};
    dependency_info.imageMemoryBarrierCount = 1;
    dependency_info.pImageMemoryBarriers = &barrier;

    VulkanCommandBuffers[frameIndex].pipelineBarrier2(dependency_info);
}

void Application::transitionImageLayout(const vk::raii::Image& image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout)
{
    auto commandBuffer = beginSingleTimeCommands();
    vk::ImageMemoryBarrier barrier;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.image = image;
    barrier.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };

    vk::PipelineStageFlags sourceStage;
    vk::PipelineStageFlags destinationStage;

    if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal)
    {
        barrier.srcAccessMask = {};
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

        sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
        destinationStage = vk::PipelineStageFlagBits::eTransfer;
    }
    else if (oldLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal)
    {
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

        sourceStage = vk::PipelineStageFlagBits::eTransfer;
        destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
    }
    else
    {
        throw std::invalid_argument("unsupported layout transition!");
    }
    commandBuffer.pipelineBarrier(sourceStage, destinationStage, {}, {}, nullptr, barrier);

	endSingleTimeCommands(commandBuffer);
}

vk::Extent2D Application::chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities)
{
    if (capabilities.currentExtent.width != 0xFFFFFFFF)
    {
        return capabilities.currentExtent;
    }
    int WindowWidth, WindowHeight;
    glfwGetFramebufferSize(Window, &WindowWidth, &WindowHeight);

    return {
        std::clamp<uint32_t>(WindowWidth, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
        std::clamp<uint32_t>(WindowHeight, capabilities.minImageExtent.height, capabilities.maxImageExtent.height) };
}

void Application::CleanupSwapChain()
{
    VulkanSwapChainImageViews.clear();
    VulkanSwapChain = nullptr;
}

void Application::MainLoop()
{
    while (!glfwWindowShouldClose(Window)) {
        glfwPollEvents(); 
    	drawFrame();
    }
    VulkanLogicalDevice.waitIdle();        // wait for device to finish operations before destroying resources
}

void Application::RecreateSwapChain()
{
    int width = 0, height = 0;
    glfwGetFramebufferSize(Window, &width, &height);
    while (width == 0 || height == 0)
    {
        glfwGetFramebufferSize(Window, &width, &height);
        glfwWaitEvents();
    }

    VulkanLogicalDevice.waitIdle();

    CleanupSwapChain();
    CreateSwapChain();
    CreateImageViews();
    CreateDepthResources();
}

void Application::Cleanup()
{
    glfwDestroyWindow(Window);

    glfwTerminate();
}

std::vector<const char*> Application::getRequiredExtensions() {
    uint32_t glfwExtensionCount = 0;
    auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
    if (enableValidationLayers) {
        extensions.push_back(vk::EXTDebugUtilsExtensionName);
    }

    return extensions;
}