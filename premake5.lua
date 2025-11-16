workspace "Lumina"
	configurations { "Debug", "Release", "Shipping" }
	targetdir "Build"
	startproject "Editor"
	conformancemode "On"

	language "C++"
	cppdialect "C++20"
	staticruntime "Off"

	flags  
	{
		"MultiProcessorCompile",
        "NoIncrementalLink",
        "ShadowedVariables",
	}
		
	defines
	{
		"EASTL_USER_DEFINED_ALLOCATOR=1",
		"_CRT_SECURE_NO_WARNINGS",
		"GLM_FORCE_DEPTH_ZERO_TO_ONE",
		"GLM_FORCE_LEFT_HANDED",
		"IMGUI_DEFINE_MATH_OPERATORS",
	}

	filter "action:vs"
	
	filter "language:C++ or language:C"
		architecture "x86_64"

	buildoptions 
    {
		"/Zm2000",
	}

	disablewarnings
    {
        "4251", -- DLL-interface warning
        "4275", -- Non-DLL-interface base class
	}

    warnings "Default"

    -- Platform-specific settings
    filter "system:windows"
        systemversion "latest"
        defines { "LE_PLATFORM_WINDOWS" }
        
        buildoptions 
        { 
            "/MP",      -- Multi-processor compilation
            "/Zc:inline", -- Remove unreferenced functions/data
            "/Zc:__cplusplus",
        }

    filter "system:linux"
        defines { "LE_PLATFORM_LINUX" }
        buildoptions { "-march=native" } -- CPU-specific optimizations

    -- Debug Configuration
    filter "configurations:Debug"
        runtime "Debug"
        links { "%{VULKAN_SDK}/lib/shaderc_combinedd.lib" }
        optimize "Debug"
        symbols "On"
        editandcontinue "Off"
        defines { "LE_DEBUG", "_DEBUG" }

    -- Release Configuration (Developer build with symbols)
    filter "configurations:Release"
        runtime "Release"
        vectorextensions "AVX2"
        isaextensions { "BMI", "POPCNT", "LZCNT", "F16C" }
        links { "%{VULKAN_SDK}/lib/shaderc_combined.lib" }
        optimize "Speed"
        symbols "On" -- Keep symbols for profiling
        defines { "LE_RELEASE", "NDEBUG" }
        flags { "LinkTimeOptimization" }
        

    -- Shipping Configuration (Maximum optimization, no symbols)
    filter "configurations:Shipping"
        runtime "Release"
        vectorextensions "AVX2"
        isaextensions { "BMI", "POPCNT", "LZCNT", "F16C" }
        links { "%{VULKAN_SDK}/lib/shaderc_combined.lib" }
        optimize "Full"
        symbols "Off"
        defines { "LE_SHIP", "NDEBUG" }
        flags { "LinkTimeOptimization" }
        

    filter {}

	group "Dependencies"
		include "Lumina/Engine/ThirdParty/EA"
		include "Lumina/Engine/ThirdParty/glfw"
		include "Lumina/Engine/ThirdParty/imgui"
		include "Lumina/Engine/Thirdparty/Tracy"
		include "Lumina/Engine/ThirdParty/xxhash"
	group ""

	group "Core"
		include "Lumina"
	group ""

	group "Applications"
		include "Lumina/Applications/LuminaEditor"
		include "Lumina/Applications/Reflector"
		include "Lumina/Applications/Sandbox"
	group ""