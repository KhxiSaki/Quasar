project "Game"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++23"
	staticruntime "off"

	targetdir ("Binaries/" .. outputdir .. "/%{prj.name}")
	objdir ("Intermediate/" .. outputdir .. "/%{prj.name}")

files { 
		"Source/**.h", 
		"Source/**.cpp"
	}

	includedirs
	{
		"Source",
		"../Engine/Source",
		"../Engine/ThirdParty/ImGui",
		"../Engine/ThirdParty/ImGui/backends",
		"%{IncludeDir.GLFW}",
		"%{IncludeDir.GLFW}/include",
		"%{IncludeDir.VulkanSDK}",
		"%{IncludeDir.GLM}",
		"%{IncludeDir.stb}",
"%{IncludeDir.tinyobjloader}",
"%{IncludeDir.tinygltf}",
"%{IncludeDir.KTX}"
	}

	links
	{
		"Engine",
		"ImGui"
	}

	filter "system:windows"
		systemversion "latest"
		defines { "CAE_PLATFORM_WINDOWS" }

	filter "configurations:Debug"
		defines { "CAE_DEBUG" }
		runtime "Debug"
		symbols "On"

	filter "configurations:Release"
		defines { "CAE_RELEASE" }
		runtime "Release"
		optimize "On"

filter "configurations:Dist"
		defines { "CAE_DIST" }
		runtime "Release"
		optimize "On"
		symbols "Off"