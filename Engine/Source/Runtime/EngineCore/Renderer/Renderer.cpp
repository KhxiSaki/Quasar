
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS

#include "Renderer.h"
#include <chrono>

#include "Runtime/EngineCore/Window.h"


//TODO: File system and ecs load objedct and set the filepath
const std::string MODEL_PATH = /*"../Engine/Content/Models/viking_room.obj"*/ "../Engine/Content/Models/viking_room.glb";
const std::string TEXTURE_PATH = "../Engine/Content/Textures/viking_room.png";

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

Renderer::Renderer(Window* InWindow)
	: RendererWindow(InWindow)
{

}

Renderer::~Renderer()
{
   
}

void Renderer::Initialize()
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
    CreateGBuffer();
    CreateGBufferDescriptorSetLayout();
    CreateGBufferPipeline();
    CreateLightingPassDescriptorSetLayout();
    CreateLightingPassPipeline();
    CreateTextureImage();
    // CreateTextureImageWithKTX();
    CreateTextureImageView();
    CreateTextureSampler();
    //LoadModel();
    LoadModelWithGLTF();
    CreateVertexBuffer();
    CreateIndexBuffer();
    CreateUniformBuffers();
    CreateDescriptorPool();
    CreateDescriptorSets();
    CreateGBufferDescriptorPool();
    CreateGBufferDescriptorSets();
    CreateLightingPassDescriptorPool();
    CreateLightingPassLightBuffers();
    CreateLightingPassDescriptorSets();
    CreateCommandBuffers();
    CreateSyncObjects();

    // Initialize ImGui
    imGui.init(VulkanLogicalDevice, VulkanPhysicalDevice, VulkanGraphicsQueue, VulkanCommandPool, queueIndex);
    imGui.setColorFormat(VulkanSwapChainSurfaceFormat.format);
    imGui.initialize(static_cast<float>(VulkanSwapChainExtent.width), static_cast<float>(VulkanSwapChainExtent.height));
    imGui.initResources();
}

void Renderer::Render()
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

    // Update ImGui display size for current window size
    imGui.updateDisplaySize(static_cast<float>(VulkanSwapChainExtent.width), static_cast<float>(VulkanSwapChainExtent.height));

    // Update ImGui
    imGui.newFrame();
    imGui.updateBuffers();

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
    if ((result == vk::Result::eSuboptimalKHR) || (result == vk::Result::eErrorOutOfDateKHR) || RendererWindow->IsResized())
    {
        RendererWindow->SetResizedFalse();
        RecreateSwapChain();
    }
    else
    {
        // There are no other success codes than eSuccess; on any error code, presentKHR already threw an exception.
        assert(result == vk::Result::eSuccess);
    }
    frameIndex = (frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Renderer::Shutdown()
{
}

void Renderer::CreateInstance()
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

void Renderer::SetupDebugMessenger()
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

void Renderer::CreateSurface()
{
    VkSurfaceKHR _surface;
    if (glfwCreateWindowSurface(*VulkanInstance, RendererWindow->getGLFWwindow(), nullptr, &_surface) != 0) {
        throw std::runtime_error("failed to create window surface!");
    }
    VulkanSurface = vk::raii::SurfaceKHR(VulkanInstance, _surface);
}

void Renderer::PickPhysicalDevice()
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

void Renderer::CreateLogicalDevice()
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

void Renderer::CreateSwapChain()
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

vk::raii::ImageView Renderer::CreateImageView(vk::raii::Image& image, vk::Format format, vk::ImageAspectFlags aspectFlags)
{
    vk::ImageViewCreateInfo viewInfo;
    viewInfo.image = image;
    viewInfo.viewType = vk::ImageViewType::e2D;
    viewInfo.format = format;
    viewInfo.subresourceRange = { aspectFlags, 0, 1, 0, 1 };

    return vk::raii::ImageView(VulkanLogicalDevice, viewInfo);
}

void Renderer::CreateImageViews()
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

void Renderer::CreateDescriptorSetLayout()
{
    //Create Uniform Buffer Object Layout Binding
    std::array bindings = {
      vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex, nullptr),
      vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, nullptr)
    };

    vk::DescriptorSetLayoutCreateInfo layoutInfo({}, bindings.size(), bindings.data());

    VulkanDescriptorSetLayout = vk::raii::DescriptorSetLayout(VulkanLogicalDevice, layoutInfo);
}

void Renderer::CreateGraphicsPipeline()
{
    vk::raii::ShaderModule VertexShaderModule = CreateShaderModule(ReadFile("../Engine/Binaries/Shaders/ShaderType_Vertex.vert.spv"));
    vk::raii::ShaderModule FragmentShaderModule = CreateShaderModule(ReadFile("../Engine/Binaries/Shaders/ShaderTypes_Fragment.frag.spv"));

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

void Renderer::CreateCommandPool()
{
    vk::CommandPoolCreateInfo poolInfo;
    poolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    poolInfo.queueFamilyIndex = queueIndex;

    VulkanCommandPool = vk::raii::CommandPool(VulkanLogicalDevice, poolInfo);
}

void Renderer::CreateDepthResources()
{
    vk::Format depthFormat = findDepthFormat();
    CreateImage(VulkanSwapChainExtent.width, VulkanSwapChainExtent.height, depthFormat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::MemoryPropertyFlagBits::eDeviceLocal, depthImage, depthImageMemory);
    depthImageView = CreateImageView(depthImage, depthFormat, vk::ImageAspectFlagBits::eDepth);
}

void Renderer::CreateGBuffer()
{
    // Create G-Buffer attachments for deferred rendering
    uint32_t width = VulkanSwapChainExtent.width;
    uint32_t height = VulkanSwapChainExtent.height;
    
    // Position buffer - RGBA32F for world space position
    CreateImage(width, height, vk::Format::eR32G32B32A32Sfloat, vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
        vk::MemoryPropertyFlagBits::eDeviceLocal, gBuffer.positionImage, gBuffer.positionImageMemory);
    gBuffer.positionImageView = CreateImageView(gBuffer.positionImage, vk::Format::eR32G32B32A32Sfloat, vk::ImageAspectFlagBits::eColor);
    
    // Normal buffer - RGBA16F for world space normal
    CreateImage(width, height, vk::Format::eR16G16B16A16Sfloat, vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
        vk::MemoryPropertyFlagBits::eDeviceLocal, gBuffer.normalImage, gBuffer.normalImageMemory);
    gBuffer.normalImageView = CreateImageView(gBuffer.normalImage, vk::Format::eR16G16B16A16Sfloat, vk::ImageAspectFlagBits::eColor);
    
    // Albedo buffer - RGBA8 for color
    CreateImage(width, height, vk::Format::eR8G8B8A8Unorm, vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
        vk::MemoryPropertyFlagBits::eDeviceLocal, gBuffer.albedoImage, gBuffer.albedoImageMemory);
    gBuffer.albedoImageView = CreateImageView(gBuffer.albedoImage, vk::Format::eR8G8B8A8Unorm, vk::ImageAspectFlagBits::eColor);
    
    // PBR buffer - RGBA8 for roughness, metallic, AO
    CreateImage(width, height, vk::Format::eR8G8B8A8Unorm, vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
        vk::MemoryPropertyFlagBits::eDeviceLocal, gBuffer.pbrImage, gBuffer.pbrImageMemory);
    gBuffer.pbrImageView = CreateImageView(gBuffer.pbrImage, vk::Format::eR8G8B8A8Unorm, vk::ImageAspectFlagBits::eColor);
}

void Renderer::CleanupGBuffer()
{
    gBuffer.positionImageView = nullptr;
    gBuffer.positionImage = nullptr;
    gBuffer.positionImageMemory = nullptr;
    
    gBuffer.normalImageView = nullptr;
    gBuffer.normalImage = nullptr;
    gBuffer.normalImageMemory = nullptr;
    
    gBuffer.albedoImageView = nullptr;
    gBuffer.albedoImage = nullptr;
    gBuffer.albedoImageMemory = nullptr;
    
    gBuffer.pbrImageView = nullptr;
    gBuffer.pbrImage = nullptr;
    gBuffer.pbrImageMemory = nullptr;
}

void Renderer::CreateGBufferDescriptorSetLayout()
{
    std::array bindings = {
        vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex, nullptr),
        vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, nullptr)
    };
    
    vk::DescriptorSetLayoutCreateInfo layoutInfo({}, bindings.size(), bindings.data());
    gBufferDescriptorSetLayout = vk::raii::DescriptorSetLayout(VulkanLogicalDevice, layoutInfo);
}

void Renderer::CreateGBufferPipeline()
{
    vk::raii::ShaderModule vertexShaderModule = CreateShaderModule(ReadFile("../Engine/Binaries/Shaders/Deferred_GeometryPass.vert.spv"));
    vk::raii::ShaderModule fragmentShaderModule = CreateShaderModule(ReadFile("../Engine/Binaries/Shaders/Deferred_GeometryPass.frag.spv"));
    
    vk::PipelineShaderStageCreateInfo vertShaderStageInfo;
    vertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
    vertShaderStageInfo.module = vertexShaderModule;
    vertShaderStageInfo.pName = "main";
    
    vk::PipelineShaderStageCreateInfo fragShaderStageInfo;
    fragShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
    fragShaderStageInfo.module = fragmentShaderModule;
    fragShaderStageInfo.pName = "main";
    
    vk::PipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };
    
    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();
    
    vk::PipelineVertexInputStateCreateInfo vertexInputInfo;
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
    
    // Multiple color attachments for G-Buffer
    std::array<vk::PipelineColorBlendAttachmentState, 4> colorBlendAttachments;
    for (auto& attachment : colorBlendAttachments) {
        attachment.blendEnable = vk::False;
        attachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    }
    
    vk::PipelineColorBlendStateCreateInfo colorBlending;
    colorBlending.logicOpEnable = vk::False;
    colorBlending.logicOp = vk::LogicOp::eCopy;
    colorBlending.attachmentCount = colorBlendAttachments.size();
    colorBlending.pAttachments = colorBlendAttachments.data();
    
    std::vector dynamicStates = {
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor };
    vk::PipelineDynamicStateCreateInfo dynamicState;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();
    
    vk::PipelineLayoutCreateInfo pipelineLayoutInfo;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &*gBufferDescriptorSetLayout;
    
    gBufferPipelineLayout = vk::raii::PipelineLayout(VulkanLogicalDevice, pipelineLayoutInfo);
    
    // Pipeline rendering info with multiple color attachments
    std::array<vk::Format, 4> colorFormats = {
        vk::Format::eR32G32B32A32Sfloat,  // Position
        vk::Format::eR16G16B16A16Sfloat,  // Normal
        vk::Format::eR8G8B8A8Unorm,       // Albedo
        vk::Format::eR8G8B8A8Unorm        // PBR
    };
    
    vk::PipelineRenderingCreateInfo pipelineRenderingInfo;
    pipelineRenderingInfo.colorAttachmentCount = colorFormats.size();
    pipelineRenderingInfo.pColorAttachmentFormats = colorFormats.data();
    pipelineRenderingInfo.depthAttachmentFormat = findDepthFormat();
    
    vk::GraphicsPipelineCreateInfo graphicsPipelineInfo;
    graphicsPipelineInfo.stageCount = 2;
    graphicsPipelineInfo.pStages = shaderStages;
    graphicsPipelineInfo.pVertexInputState = &vertexInputInfo;
    graphicsPipelineInfo.pInputAssemblyState = &inputAssembly;
    graphicsPipelineInfo.pViewportState = &viewportState;
    graphicsPipelineInfo.pRasterizationState = &rasterizer;
    graphicsPipelineInfo.pMultisampleState = &multisampling;
    graphicsPipelineInfo.pDepthStencilState = &depthStencil;
    graphicsPipelineInfo.pColorBlendState = &colorBlending;
    graphicsPipelineInfo.pDynamicState = &dynamicState;
    graphicsPipelineInfo.layout = gBufferPipelineLayout;
    graphicsPipelineInfo.renderPass = nullptr;
    graphicsPipelineInfo.pNext = &pipelineRenderingInfo;
    
    gBufferPipeline = vk::raii::Pipeline(VulkanLogicalDevice, nullptr, graphicsPipelineInfo);
}

void Renderer::CreateLightingPassDescriptorSetLayout()
{
    std::array bindings = {
        vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, nullptr),  // gPosition
        vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, nullptr),  // gNormal
        vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, nullptr),  // gAlbedo
        vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, nullptr),  // gPBR
        vk::DescriptorSetLayoutBinding(4, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eFragment, nullptr)          // LightData
    };
    
    vk::DescriptorSetLayoutCreateInfo layoutInfo({}, bindings.size(), bindings.data());
    lightingPassDescriptorSetLayout = vk::raii::DescriptorSetLayout(VulkanLogicalDevice, layoutInfo);
}

void Renderer::CreateLightingPassPipeline()
{
    vk::raii::ShaderModule vertexShaderModule = CreateShaderModule(ReadFile("../Engine/Binaries/Shaders/Deferred_LightingPass.vert.spv"));
    vk::raii::ShaderModule fragmentShaderModule = CreateShaderModule(ReadFile("../Engine/Binaries/Shaders/Deferred_LightingPass.frag.spv"));
    
    vk::PipelineShaderStageCreateInfo vertShaderStageInfo;
    vertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
    vertShaderStageInfo.module = vertexShaderModule;
    vertShaderStageInfo.pName = "main";
    
    vk::PipelineShaderStageCreateInfo fragShaderStageInfo;
    fragShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
    fragShaderStageInfo.module = fragmentShaderModule;
    fragShaderStageInfo.pName = "main";
    
    vk::PipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };
    
    // No vertex input for full-screen quad (using gl_VertexIndex)
    vk::PipelineVertexInputStateCreateInfo vertexInputInfo;
    vertexInputInfo.vertexBindingDescriptionCount = 0;
    vertexInputInfo.pVertexBindingDescriptions = nullptr;
    vertexInputInfo.vertexAttributeDescriptionCount = 0;
    vertexInputInfo.pVertexAttributeDescriptions = nullptr;
    
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
    rasterizer.cullMode = vk::CullModeFlagBits::eNone;  // No culling for full-screen quad
    rasterizer.frontFace = vk::FrontFace::eCounterClockwise;
    rasterizer.depthBiasEnable = vk::False;
    rasterizer.lineWidth = 1.0f;
    
    vk::PipelineMultisampleStateCreateInfo multisampling;
    multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;
    multisampling.sampleShadingEnable = vk::False;
    
    // No depth testing for lighting pass
    vk::PipelineDepthStencilStateCreateInfo depthStencil;
    depthStencil.depthTestEnable = vk::False;
    depthStencil.depthWriteEnable = vk::False;
    depthStencil.depthCompareOp = vk::CompareOp::eAlways;
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
    pipelineLayoutInfo.pSetLayouts = &*lightingPassDescriptorSetLayout;
    
    lightingPassPipelineLayout = vk::raii::PipelineLayout(VulkanLogicalDevice, pipelineLayoutInfo);
    
    vk::PipelineRenderingCreateInfo pipelineRenderingInfo;
    pipelineRenderingInfo.colorAttachmentCount = 1;
    pipelineRenderingInfo.pColorAttachmentFormats = &VulkanSwapChainSurfaceFormat.format;
    
    vk::GraphicsPipelineCreateInfo graphicsPipelineInfo;
    graphicsPipelineInfo.stageCount = 2;
    graphicsPipelineInfo.pStages = shaderStages;
    graphicsPipelineInfo.pVertexInputState = &vertexInputInfo;
    graphicsPipelineInfo.pInputAssemblyState = &inputAssembly;
    graphicsPipelineInfo.pViewportState = &viewportState;
    graphicsPipelineInfo.pRasterizationState = &rasterizer;
    graphicsPipelineInfo.pMultisampleState = &multisampling;
    graphicsPipelineInfo.pDepthStencilState = &depthStencil;
    graphicsPipelineInfo.pColorBlendState = &colorBlending;
    graphicsPipelineInfo.pDynamicState = &dynamicState;
    graphicsPipelineInfo.layout = lightingPassPipelineLayout;
    graphicsPipelineInfo.renderPass = nullptr;
    graphicsPipelineInfo.pNext = &pipelineRenderingInfo;
    
    lightingPassPipeline = vk::raii::Pipeline(VulkanLogicalDevice, nullptr, graphicsPipelineInfo);
}

void Renderer::CreateTextureImage()
{
    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load(TEXTURE_PATH.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
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

void Renderer::CreateTextureImageWithKTX() {
    /*// Load KTX2 texture instead of using stb_image
    ktxTexture* kTexture;
    KTX_error_code result = ktxTexture_CreateFromNamedFile(
        TEXTURE_PATH.c_str(),
        KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
        &kTexture);

    if (result != KTX_SUCCESS)
    {
        throw std::runtime_error("failed to load ktx texture image!");
    }

    // Get texture dimensions and data
    uint32_t     texWidth = kTexture->baseWidth;
    uint32_t     texHeight = kTexture->baseHeight;
    ktx_size_t   imageSize = ktxTexture_GetImageSize(kTexture, 0);
    ktx_uint8_t* ktxTextureData = ktxTexture_GetData(kTexture);

    vk::raii::Buffer       stagingBuffer({});
    vk::raii::DeviceMemory stagingBufferMemory({});
    createBuffer(imageSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBuffer, stagingBufferMemory);

    void* data = stagingBufferMemory.mapMemory(0, imageSize);
    memcpy(data, ktxTextureData, imageSize);
    stagingBufferMemory.unmapMemory();

    // Determine the Vulkan format from KTX format
    vk::Format textureFormat;

    // Check if the KTX texture has a format
    if (kTexture->classId == ktxTexture2_c)
    {
        // For KTX2 files, we can get the format directly
        auto* ktx2 = reinterpret_cast<ktxTexture2*>(kTexture);
        textureFormat = static_cast<vk::Format>(ktx2->vkFormat);
        if (textureFormat == vk::Format::eUndefined)
        {
            // If the format is undefined, fall back to a reasonable default
            textureFormat = vk::Format::eR8G8B8A8Unorm;
        }
    }
    else
    {
        // For KTX1 files or if we can't determine the format, use a reasonable default
        textureFormat = vk::Format::eR8G8B8A8Unorm;
    }

    textureImageFormat = textureFormat;

    createImage(texWidth, texHeight, textureFormat, vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
        vk::MemoryPropertyFlagBits::eDeviceLocal, textureImage, textureImageMemory);

    transitionImageLayout(textureImage, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
    copyBufferToImage(stagingBuffer, textureImage, texWidth, texHeight);
    transitionImageLayout(textureImage, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);

    ktxTexture_Destroy(kTexture);*/
}

void Renderer::CreateImage(uint32_t width, uint32_t height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties, vk::raii::Image& image, vk::raii::DeviceMemory& imageMemory)
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

void Renderer::CreateTextureImageView()
{
    textureImageView = CreateImageView(textureImage, vk::Format::eR8G8B8A8Srgb, vk::ImageAspectFlagBits::eColor);
}

void Renderer::CreateTextureSampler()
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

void Renderer::LoadModel()
{
    /*tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &err, MODEL_PATH.c_str())) {
        throw std::runtime_error(err);
    }

    std::unordered_map<Vertex, uint32_t> uniqueVertices{};

    for (const auto& shape : shapes)
    {
        for (const auto& index : shape.mesh.indices)
        {
            Vertex vertex{};

            vertex.pos = {
                 attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };

            vertex.texCoord = {
                attrib.texcoords[2 * index.texcoord_index + 0],
                1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
            };

            vertex.color = { 1.0f, 1.0f, 1.0f };

            if (!uniqueVertices.contains(vertex))
            {
                uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
                vertices.push_back(vertex);
            }

            indices.push_back(uniqueVertices[vertex]);
        }
    }*/


}

void Renderer::LoadModelWithGLTF() {
    // Use tinygltf to load the model instead of tinyobjloader
    tinygltf::Model    model;
    tinygltf::TinyGLTF loader;
    std::string        err;
    std::string        warn;

    bool ret = loader.LoadBinaryFromFile(&model, &err, &warn, MODEL_PATH);

    if (!warn.empty())
    {
        std::cout << "glTF warning: " << warn << std::endl;
    }

    if (!err.empty())
    {
        std::cout << "glTF error: " << err << std::endl;
    }

    if (!ret)
    {
        throw std::runtime_error("Failed to load glTF model");
    }

    vertices.clear();
    indices.clear();

    // Process all meshes in the model
    for (const auto& mesh : model.meshes)
    {
        for (const auto& primitive : mesh.primitives)
        {
            // Get indices
            const tinygltf::Accessor& indexAccessor = model.accessors[primitive.indices];
            const tinygltf::BufferView& indexBufferView = model.bufferViews[indexAccessor.bufferView];
            const tinygltf::Buffer& indexBuffer = model.buffers[indexBufferView.buffer];

            // Get vertex positions
            const tinygltf::Accessor& posAccessor = model.accessors[primitive.attributes.at("POSITION")];
            const tinygltf::BufferView& posBufferView = model.bufferViews[posAccessor.bufferView];
            const tinygltf::Buffer& posBuffer = model.buffers[posBufferView.buffer];

            // Get texture coordinates if available
            bool                        hasTexCoords = primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end();
            const tinygltf::Accessor* texCoordAccessor = nullptr;
            const tinygltf::BufferView* texCoordBufferView = nullptr;
            const tinygltf::Buffer* texCoordBuffer = nullptr;

            if (hasTexCoords)
            {
                texCoordAccessor = &model.accessors[primitive.attributes.at("TEXCOORD_0")];
                texCoordBufferView = &model.bufferViews[texCoordAccessor->bufferView];
                texCoordBuffer = &model.buffers[texCoordBufferView->buffer];
            }

            uint32_t baseVertex = static_cast<uint32_t>(vertices.size());

            for (size_t i = 0; i < posAccessor.count; i++)
            {
                Vertex vertex{};

                const float* pos = reinterpret_cast<const float*>(&posBuffer.data[posBufferView.byteOffset + posAccessor.byteOffset + i * 12]);
                // Convert from glTF Y-up to Vulkan Y-down (Z-up for Vulkan)
                vertex.pos = { pos[0], -pos[2], pos[1] };

                if (hasTexCoords)
                {
                    const float* texCoord = reinterpret_cast<const float*>(&texCoordBuffer->data[texCoordBufferView->byteOffset + texCoordAccessor->byteOffset + i * 8]);
                    vertex.texCoord = { texCoord[0], texCoord[1] };
                }
                else
                {
                    vertex.texCoord = { 0.0f, 0.0f };
                }

                vertex.color = { 1.0f, 1.0f, 1.0f };

                vertices.push_back(vertex);
            }

            const unsigned char* indexData = &indexBuffer.data[indexBufferView.byteOffset + indexAccessor.byteOffset];
            size_t               indexCount = indexAccessor.count;
            size_t               indexStride = 0;

            // Determine index stride based on component type
            if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
            {
                indexStride = sizeof(uint16_t);
            }
            else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
            {
                indexStride = sizeof(uint32_t);
            }
            else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
            {
                indexStride = sizeof(uint8_t);
            }
            else
            {
                throw std::runtime_error("Unsupported index component type");
            }

            indices.reserve(indices.size() + indexCount);

            for (size_t i = 0; i < indexCount; i++)
            {
                uint32_t index = 0;

                if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
                {
                    index = *reinterpret_cast<const uint16_t*>(indexData + i * indexStride);
                }
                else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
                {
                    index = *reinterpret_cast<const uint32_t*>(indexData + i * indexStride);
                }
                else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
                {
                    index = *reinterpret_cast<const uint8_t*>(indexData + i * indexStride);
                }

                indices.push_back(baseVertex + index);
            }
        }
    }
}

void Renderer::CreateBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, vk::raii::Buffer& buffer, vk::raii::DeviceMemory& bufferMemory)
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

void Renderer::CreateDescriptorPool()
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

void Renderer::CreateDescriptorSets()
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

void Renderer::CreateGBufferDescriptorPool()
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
    
    gBufferDescriptorPool = vk::raii::DescriptorPool(VulkanLogicalDevice, poolInfo);
}

void Renderer::CreateGBufferDescriptorSets()
{
    // Free existing descriptor sets if any
    if (!gBufferDescriptorSets.empty()) {
        //VulkanLogicalDevice.(gBufferDescriptorPool, gBufferDescriptorSets);
        gBufferDescriptorSets.clear();
    }
    
    std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, *gBufferDescriptorSetLayout);
    vk::DescriptorSetAllocateInfo allocInfo;
    allocInfo.descriptorPool = gBufferDescriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
    allocInfo.pSetLayouts = layouts.data();
    
    gBufferDescriptorSets = VulkanLogicalDevice.allocateDescriptorSets(allocInfo);
    
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
        
        vk::WriteDescriptorSet bufferDescriptorWrite;
        bufferDescriptorWrite.dstSet = gBufferDescriptorSets[i];
        bufferDescriptorWrite.dstBinding = 0;
        bufferDescriptorWrite.dstArrayElement = 0;
        bufferDescriptorWrite.descriptorCount = 1;
        bufferDescriptorWrite.descriptorType = vk::DescriptorType::eUniformBuffer;
        bufferDescriptorWrite.pBufferInfo = &bufferInfo;
        
        vk::WriteDescriptorSet imageDescriptorWrite;
        imageDescriptorWrite.dstSet = gBufferDescriptorSets[i];
        imageDescriptorWrite.dstBinding = 1;
        imageDescriptorWrite.dstArrayElement = 0;
        imageDescriptorWrite.descriptorCount = 1;
        imageDescriptorWrite.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        imageDescriptorWrite.pImageInfo = &imageInfo;
        
        std::array descriptorWrites{ bufferDescriptorWrite, imageDescriptorWrite };
        VulkanLogicalDevice.updateDescriptorSets(descriptorWrites, {});
    }
}

void Renderer::CreateLightingPassDescriptorPool()
{
    std::array poolSize{
        vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, MAX_FRAMES_IN_FLIGHT * 4),  // 4 G-Buffer textures
        vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, MAX_FRAMES_IN_FLIGHT)  // Light data
    };
    
    vk::DescriptorPoolCreateInfo poolInfo;
    poolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    poolInfo.maxSets = MAX_FRAMES_IN_FLIGHT;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSize.size());
    poolInfo.pPoolSizes = poolSize.data();
    
    lightingPassDescriptorPool = vk::raii::DescriptorPool(VulkanLogicalDevice, poolInfo);
}

void Renderer::CreateLightingPassDescriptorSets()
{
    // Free existing descriptor sets if any
    if (!lightingPassDescriptorSets.empty()) {
       // VulkanLogicalDevice.freeDescriptorSets(lightingPassDescriptorPool, lightingPassDescriptorSets);
        lightingPassDescriptorSets.clear();
    }
    
    std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, *lightingPassDescriptorSetLayout);
    vk::DescriptorSetAllocateInfo allocInfo;
    allocInfo.descriptorPool = lightingPassDescriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
    allocInfo.pSetLayouts = layouts.data();
    
    lightingPassDescriptorSets = VulkanLogicalDevice.allocateDescriptorSets(allocInfo);
    
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        std::array<vk::DescriptorImageInfo, 4> imageInfos;
        
        // gPosition
        imageInfos[0].sampler = textureSampler;
        imageInfos[0].imageView = gBuffer.positionImageView;
        imageInfos[0].imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        
        // gNormal
        imageInfos[1].sampler = textureSampler;
        imageInfos[1].imageView = gBuffer.normalImageView;
        imageInfos[1].imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        
        // gAlbedo
        imageInfos[2].sampler = textureSampler;
        imageInfos[2].imageView = gBuffer.albedoImageView;
        imageInfos[2].imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        
        // gPBR
        imageInfos[3].sampler = textureSampler;
        imageInfos[3].imageView = gBuffer.pbrImageView;
        imageInfos[3].imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        
        vk::DescriptorBufferInfo lightBufferInfo;
        lightBufferInfo.buffer = lightingPassLightBuffers[i];
        lightBufferInfo.offset = 0;
        lightBufferInfo.range = sizeof(LightData);
        
        std::array<vk::WriteDescriptorSet, 5> descriptorWrites;
        
        // G-Buffer image descriptors
        for (size_t j = 0; j < 4; j++)
        {
            descriptorWrites[j].dstSet = lightingPassDescriptorSets[i];
            descriptorWrites[j].dstBinding = j;
            descriptorWrites[j].dstArrayElement = 0;
            descriptorWrites[j].descriptorCount = 1;
            descriptorWrites[j].descriptorType = vk::DescriptorType::eCombinedImageSampler;
            descriptorWrites[j].pImageInfo = &imageInfos[j];
        }
        
        // Light data descriptor
        descriptorWrites[4].dstSet = lightingPassDescriptorSets[i];
        descriptorWrites[4].dstBinding = 4;
        descriptorWrites[4].dstArrayElement = 0;
        descriptorWrites[4].descriptorCount = 1;
        descriptorWrites[4].descriptorType = vk::DescriptorType::eUniformBuffer;
        descriptorWrites[4].pBufferInfo = &lightBufferInfo;
        
        VulkanLogicalDevice.updateDescriptorSets(descriptorWrites, {});
    }
}

void Renderer::CreateLightingPassLightBuffers()
{
    lightingPassLightBuffers.clear();
    lightingPassLightBuffersMemory.clear();
    lightingPassLightBuffersMapped.clear();
    
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vk::DeviceSize bufferSize = sizeof(LightData);
        vk::raii::Buffer buffer({});
        vk::raii::DeviceMemory bufferMem({});
        CreateBuffer(bufferSize, vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, buffer, bufferMem);
        lightingPassLightBuffers.emplace_back(std::move(buffer));
        lightingPassLightBuffersMemory.emplace_back(std::move(bufferMem));
        lightingPassLightBuffersMapped.emplace_back(lightingPassLightBuffersMemory[i].mapMemory(0, bufferSize));
        
        // Initialize with default light data
        LightData lightData{};
        lightData.lightPos = glm::vec3(0.0f, 0.0f, 2.0f);
        lightData.lightRadius = 10.0f;
        lightData.lightColor = glm::vec3(1.0f, 1.0f, 1.0f);
        lightData.viewPos = glm::vec3(2.0f, 2.0f, 2.0f);
        lightData.exposure = 1.0f;
        
        memcpy(lightingPassLightBuffersMapped[i], &lightData, sizeof(lightData));
    }
}

void Renderer::CreateVertexBuffer()
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

void Renderer::CreateIndexBuffer()
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

void Renderer::CreateUniformBuffers()
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

void Renderer::CreateCommandBuffers()
{
    VulkanCommandBuffers.clear();

    vk::CommandBufferAllocateInfo allocInfo;
    allocInfo.commandPool = VulkanCommandPool;
    allocInfo.level = vk::CommandBufferLevel::ePrimary;
    allocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;

    VulkanCommandBuffers = vk::raii::CommandBuffers(VulkanLogicalDevice, allocInfo);
}

void Renderer::CreateSyncObjects()
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

void Renderer::UpdateUniformBuffer(uint32_t currentImage)
{
    static auto startTime = std::chrono::high_resolution_clock::now();

    auto  currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float>(currentTime - startTime).count();

    UniformBufferObject ubo{};
    ubo.model = rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.view = lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.proj = glm::perspective(glm::radians(45.0f), static_cast<float>(VulkanSwapChainExtent.width) / static_cast<float>(VulkanSwapChainExtent.height), 0.1f, 10.0f);
    ubo.proj[1][1] *= -1;
    ubo.viewPos = glm::vec3(2.0f, 2.0f, 2.0f);
    ubo.lightPos = glm::vec3(0.0f, 0.0f, 2.0f);
    ubo.lightColor = glm::vec3(1.0f, 1.0f, 1.0f);
    ubo.lightRadius = 10.0f;
    ubo.exposure = 1.0f;

    memcpy(VulkanUniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
    
    // Update lighting pass light data
    LightData lightData{};
    lightData.lightPos = ubo.lightPos;
    lightData.lightRadius = ubo.lightRadius;
    lightData.lightColor = ubo.lightColor;
    lightData.viewPos = ubo.viewPos;
    lightData.exposure = ubo.exposure;
    
    memcpy(lightingPassLightBuffersMapped[currentImage], &lightData, sizeof(lightData));
}

void Renderer::recordCommandBuffer(uint32_t imageIndex)
{
    auto& commandBuffer = VulkanCommandBuffers[frameIndex];
    commandBuffer.begin({});
    
    // Transition the swapchain image to COLOR_ATTACHMENT_OPTIMAL
    transition_image_layout(
        VulkanSwapChainImages[imageIndex],
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eColorAttachmentOptimal,
        {},
        vk::AccessFlagBits2::eColorAttachmentWrite,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
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
    
    // Transition G-Buffer images to color attachment optimal
    transition_image_layout(
        *gBuffer.positionImage,
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eColorAttachmentOptimal,
        {},
        vk::AccessFlagBits2::eColorAttachmentWrite,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::ImageAspectFlagBits::eColor);
    
    transition_image_layout(
        *gBuffer.normalImage,
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eColorAttachmentOptimal,
        {},
        vk::AccessFlagBits2::eColorAttachmentWrite,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::ImageAspectFlagBits::eColor);
    
    transition_image_layout(
        *gBuffer.albedoImage,
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eColorAttachmentOptimal,
        {},
        vk::AccessFlagBits2::eColorAttachmentWrite,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::ImageAspectFlagBits::eColor);
    
    transition_image_layout(
        *gBuffer.pbrImage,
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eColorAttachmentOptimal,
        {},
        vk::AccessFlagBits2::eColorAttachmentWrite,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::ImageAspectFlagBits::eColor);
    
    // ==================== GEOMETRY PASS ====================
    RecordGeometryPass(imageIndex);
    
    // Transition G-Buffer images to shader read optimal for lighting pass
    transition_image_layout(
        *gBuffer.positionImage,
        vk::ImageLayout::eColorAttachmentOptimal,
        vk::ImageLayout::eShaderReadOnlyOptimal,
        vk::AccessFlagBits2::eColorAttachmentWrite,
        vk::AccessFlagBits2::eShaderRead,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::PipelineStageFlagBits2::eFragmentShader,
        vk::ImageAspectFlagBits::eColor);
    
    transition_image_layout(
        *gBuffer.normalImage,
        vk::ImageLayout::eColorAttachmentOptimal,
        vk::ImageLayout::eShaderReadOnlyOptimal,
        vk::AccessFlagBits2::eColorAttachmentWrite,
        vk::AccessFlagBits2::eShaderRead,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::PipelineStageFlagBits2::eFragmentShader,
        vk::ImageAspectFlagBits::eColor);
    
    transition_image_layout(
        *gBuffer.albedoImage,
        vk::ImageLayout::eColorAttachmentOptimal,
        vk::ImageLayout::eShaderReadOnlyOptimal,
        vk::AccessFlagBits2::eColorAttachmentWrite,
        vk::AccessFlagBits2::eShaderRead,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::PipelineStageFlagBits2::eFragmentShader,
        vk::ImageAspectFlagBits::eColor);
    
    transition_image_layout(
        *gBuffer.pbrImage,
        vk::ImageLayout::eColorAttachmentOptimal,
        vk::ImageLayout::eShaderReadOnlyOptimal,
        vk::AccessFlagBits2::eColorAttachmentWrite,
        vk::AccessFlagBits2::eShaderRead,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::PipelineStageFlagBits2::eFragmentShader,
        vk::ImageAspectFlagBits::eColor);
    
    // ==================== LIGHTING PASS ====================
    RecordLightingPass(imageIndex);
    
    // Render ImGui on top
    imGui.drawFrame(commandBuffer, VulkanSwapChainImageViews[imageIndex], VulkanSwapChainExtent);
    
    // Transition the swapchain image to PRESENT_SRC
    transition_image_layout(
        VulkanSwapChainImages[imageIndex],
        vk::ImageLayout::eColorAttachmentOptimal,
        vk::ImageLayout::ePresentSrcKHR,
        vk::AccessFlagBits2::eColorAttachmentWrite,
        {},
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::PipelineStageFlagBits2::eBottomOfPipe,
        vk::ImageAspectFlagBits::eColor);
    
    commandBuffer.end();
}

void Renderer::RecordGeometryPass(uint32_t imageIndex)
{
    auto& commandBuffer = VulkanCommandBuffers[frameIndex];

    vk::ClearValue clearPosition = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
    vk::ClearValue clearNormal = vk::ClearColorValue(0.5f, 0.5f, 1.0f, 1.0f);  // Normalized to [0,1], (0,0,1) in world space
    vk::ClearValue clearAlbedo = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
    vk::ClearValue clearPBR = vk::ClearColorValue(0.5f, 0.0f, 1.0f, 1.0f);  // roughness=0.5, metallic=0, ao=1
    vk::ClearValue clearDepth = vk::ClearDepthStencilValue(1.0f, 0);
    
    std::array<vk::RenderingAttachmentInfo, 4> colorAttachments;
    
    // Position attachment
    colorAttachments[0].setImageView(gBuffer.positionImageView);
    colorAttachments[0].setImageLayout(vk::ImageLayout::eColorAttachmentOptimal);
    colorAttachments[0].setLoadOp(vk::AttachmentLoadOp::eClear);
    colorAttachments[0].setStoreOp(vk::AttachmentStoreOp::eStore);
    colorAttachments[0].setClearValue(clearPosition);
    
    // Normal attachment
    colorAttachments[1].setImageView(gBuffer.normalImageView);
    colorAttachments[1].setImageLayout(vk::ImageLayout::eColorAttachmentOptimal);
    colorAttachments[1].setLoadOp(vk::AttachmentLoadOp::eClear);
    colorAttachments[1].setStoreOp(vk::AttachmentStoreOp::eStore);
    colorAttachments[1].setClearValue(clearNormal);
    
    // Albedo attachment
    colorAttachments[2].setImageView(gBuffer.albedoImageView);
    colorAttachments[2].setImageLayout(vk::ImageLayout::eColorAttachmentOptimal);
    colorAttachments[2].setLoadOp(vk::AttachmentLoadOp::eClear);
    colorAttachments[2].setStoreOp(vk::AttachmentStoreOp::eStore);
    colorAttachments[2].setClearValue(clearAlbedo);
    
    // PBR attachment
    colorAttachments[3].setImageView(gBuffer.pbrImageView);
    colorAttachments[3].setImageLayout(vk::ImageLayout::eColorAttachmentOptimal);
    colorAttachments[3].setLoadOp(vk::AttachmentLoadOp::eClear);
    colorAttachments[3].setStoreOp(vk::AttachmentStoreOp::eStore);
    colorAttachments[3].setClearValue(clearPBR);
    
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
    renderingInfo.colorAttachmentCount = colorAttachments.size();
    renderingInfo.pColorAttachments = colorAttachments.data();
    renderingInfo.pDepthAttachment = &depthAttachmentInfo;
    
    commandBuffer.beginRendering(renderingInfo);
    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *gBufferPipeline);
    commandBuffer.setViewport(0, vk::Viewport(0.0f, 0.0f, static_cast<float>(VulkanSwapChainExtent.width), static_cast<float>(VulkanSwapChainExtent.height), 0.0f, 1.0f));
    commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), VulkanSwapChainExtent));
    commandBuffer.bindVertexBuffers(0, *VulkanVertexBuffer, { 0 });
    commandBuffer.bindIndexBuffer(*VulkanIndexBuffer, 0, vk::IndexType::eUint32);
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, gBufferPipelineLayout, 0, *gBufferDescriptorSets[frameIndex], nullptr);
    commandBuffer.drawIndexed(indices.size(), 1, 0, 0, 0);
    commandBuffer.endRendering();
}

void Renderer::RecordLightingPass(uint32_t imageIndex)
{
    auto& commandBuffer = VulkanCommandBuffers[frameIndex];

    // Clear to dark gray instead of pure black to see if lighting is working
    vk::ClearValue clearColor = vk::ClearColorValue(0.1f, 0.1f, 0.1f, 1.0f);

    vk::RenderingAttachmentInfo colorAttachmentInfo;
    colorAttachmentInfo.setImageView(VulkanSwapChainImageViews[imageIndex]);
    colorAttachmentInfo.setImageLayout(vk::ImageLayout::eColorAttachmentOptimal);
    colorAttachmentInfo.setLoadOp(vk::AttachmentLoadOp::eClear);
    colorAttachmentInfo.setStoreOp(vk::AttachmentStoreOp::eStore);
    colorAttachmentInfo.setClearValue(clearColor);

    vk::RenderingInfo renderingInfo;
    vk::Rect2D rect2d;
    rect2d.offset = vk::Offset2D{ 0, 0 };
    rect2d.extent = VulkanSwapChainExtent;

    renderingInfo.renderArea = rect2d;
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachmentInfo;

    commandBuffer.beginRendering(renderingInfo);
    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *lightingPassPipeline);
    commandBuffer.setViewport(0, vk::Viewport(0.0f, 0.0f, static_cast<float>(VulkanSwapChainExtent.width), static_cast<float>(VulkanSwapChainExtent.height), 0.0f, 1.0f));
    commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), VulkanSwapChainExtent));
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, lightingPassPipelineLayout, 0, *lightingPassDescriptorSets[frameIndex], nullptr);
    commandBuffer.draw(3, 1, 0, 0);  // Draw full-screen triangle
    commandBuffer.endRendering();
}

void Renderer::transition_image_layout(
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

void Renderer::transitionImageLayout(const vk::raii::Image& image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout)
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

vk::Extent2D Renderer::chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities)
{
    if (capabilities.currentExtent.width != 0xFFFFFFFF)
    {
        return capabilities.currentExtent;
    }
    int WindowWidth, WindowHeight;
    glfwGetFramebufferSize(RendererWindow->getGLFWwindow(), &WindowWidth, &WindowHeight);

    return {
        std::clamp<uint32_t>(WindowWidth, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
        std::clamp<uint32_t>(WindowHeight, capabilities.minImageExtent.height, capabilities.maxImageExtent.height) };
}

void Renderer::CleanupSwapChain()
{
    VulkanSwapChainImageViews.clear();
    VulkanSwapChain = nullptr;
}

void Renderer::RecreateSwapChain()
{
    int width = 0, height = 0;
    glfwGetFramebufferSize(RendererWindow->getGLFWwindow(), &width, &height);
    while (width == 0 || height == 0)
    {
        glfwGetFramebufferSize(RendererWindow->getGLFWwindow(), &width, &height);
        glfwWaitEvents();
    }

    VulkanLogicalDevice.waitIdle();

    CleanupSwapChain();
    CleanupGBuffer();
    CreateSwapChain();
    CreateImageViews();
    CreateDepthResources();
    CreateGBuffer();
    
    // Update lighting pass descriptor sets with new G-Buffer image views
    CreateLightingPassDescriptorSets();
}

std::vector<const char*> Renderer::getRequiredExtensions() {
    uint32_t glfwExtensionCount = 0;
    auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
    if (enableValidationLayers) {
        extensions.push_back(vk::EXTDebugUtilsExtensionName);
    }

    return extensions;
}