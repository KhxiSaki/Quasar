workspace "CreationArtEngine"
	architecture "x86_64"
	startproject "Game"

	configurations
	{
		"Debug",
		"Release",
		"Dist"
	}

	flags
	{
		"MultiProcessorCompile"
	}

outputdir = "%{cfg.buildcfg}/%{cfg.system}/%{cfg.architecture}"

IncludeDir = {}
IncludeDir["GLFW"] = "%{wks.location}/Engine/ThirdParty/glfw"
IncludeDir["VulkanSDK"] = "C:/VulkanSDK/1.4.335.0/Include"
IncludeDir["GLM"] = "%{wks.location}/Engine/ThirdParty/GLM"
IncludeDir["ImGui"] = "%{wks.location}/Engine/ThirdParty/ImGui"
IncludeDir["VMA"] = "%{wks.location}/Engine/ThirdParty/VulkanMemoryAllocator"
IncludeDir["stb"] = "%{wks.location}/Engine/ThirdParty/stb"
IncludeDir["tinyobjloader"] = "%{wks.location}/Engine/ThirdParty/tinyobjloader"
IncludeDir["tinygltf"] = "%{wks.location}/Engine/ThirdParty/tinygltf"
IncludeDir["KTX"] = "%{wks.location}/Engine/ThirdParty/KTX/include"

VulkanSDK = {}
VulkanSDK.LibraryDir = "C:/VulkanSDK/1.4.335.0/Lib"

-- Validate Vulkan SDK path
if not os.isdir(VulkanSDK.LibraryDir) then
    error("Vulkan SDK not found! Please install it and set VULKAN_SDK environment variable")
end

group "Engine"
include "Engine"
group ""

group "Game"
include "Game"
group ""

group "ThirdParty"
include "Engine/ThirdParty"
group ""