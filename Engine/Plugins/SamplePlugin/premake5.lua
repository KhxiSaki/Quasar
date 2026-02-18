project "SamplePlugin"
    kind "SharedLib"
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
        "../../../Engine/Source",
    }

    filter "system:windows"
        systemversion "latest"
        defines { "CAE_PLATFORM_WINDOWS" }

    filter "configurations:Debug"
        runtime "Debug"
        symbols "on"

    filter "configurations:Release"
        runtime "Release"
        optimize "on"

    filter "configurations:Dist"
        runtime "Release"
        optimize "off"
