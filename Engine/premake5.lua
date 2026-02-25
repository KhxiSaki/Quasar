project "Engine"
	kind "StaticLib"
	language "C++"
	cppdialect "C++23"
	staticruntime "off"

	targetdir ("Binaries/" .. outputdir .. "/")
	objdir ("Intermediate/" .. outputdir .. "/")

files
	{
		"Source/**.h",
		"Source/**.cpp"
	}
	
includedirs
	{
		"Source",
		"%{IncludeDir.GLFW}",
		"%{IncludeDir.GLFW}/include",
		"%{IncludeDir.ImGui}",
		"%{IncludeDir.ImGui}/backends",
		"%{IncludeDir.VulkanSDK}",
		"%{IncludeDir.GLM}",
		"%{IncludeDir.VMA}",
		"%{IncludeDir.VMA}/include",
		"%{IncludeDir.stb}",
"%{IncludeDir.tinyobjloader}",
"%{IncludeDir.tinygltf}",
"%{IncludeDir.KTX}"
	}
	
	links
	{
		"ImGui",
		"GLFW",
		"vulkan-1"
	}

	-- Add Vulkan library directory
	filter {"system:windows", "configurations:*"}
		libdirs 
		{
			"%{VulkanSDK.LibraryDir}"
		}
	
filter "system:windows"
		systemversion "latest"

defines
	{
		"CAE_PLATFORM_WINDOWS",
		"VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS"
	}

	filter "configurations:Debug"
		runtime "Debug"
		symbols "on"

	filter "configurations:Release"
		runtime "Release"
		optimize "on"

	filter "configurations:Distribution"
		runtime "Release"
		optimize "off"