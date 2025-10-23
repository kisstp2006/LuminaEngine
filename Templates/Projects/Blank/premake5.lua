-- Path to the engine root (set via environment variable)
LuminaEngineDirectory = os.getenv("LUMINA_DIR")
outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"
include(os.getenv("LUMINA_DIR") .. "/Dependencies.lua")

workspace "$PROJECT_NAME"
    configurations { "Debug", "Release", "Shipping" }
    architecture "x86_64"
    startproject "$PROJECT_NAME"
    conformancemode "On"

    language "C++"
    cppdialect "C++20"
    staticruntime "Off"

    flags  
    {
        "MultiProcessorCompile", 
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
        flags { "MultiProcessorCompile" } -- Parallel compilation
        buildoptions { 
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
        flags { "NoRuntimeChecks", "NoIncrementalLink" }

    -- Release Configuration (Developer build with symbols)
    filter "configurations:Release"
        runtime "Release"
        links { "%{VULKAN_SDK}/lib/shaderc_combined.lib" }
        optimize "Speed"
        symbols "On" -- Keep symbols for profiling
        defines { "LE_RELEASE", "NDEBUG" }
        flags { "LinkTimeOptimization", "NoIncrementalLink" }
        

    -- Shipping Configuration (Maximum optimization, no symbols)
    filter "configurations:Shipping"
        runtime "Release"
        links { "%{VULKAN_SDK}/lib/shaderc_combined.lib" }
        optimize "Full"
        symbols "Off"
        defines { "LE_SHIP", "NDEBUG" }
        flags { "LinkTimeOptimization", "NoIncrementalLink", "NoRuntimeChecks" }



-- =============================
-- Game Project
-- =============================


project "$PROJECT_NAME"
    kind "SharedLib"
    language "C++"

    targetdir ("Binaries/%{cfg.buildcfg}")
    objdir ("Intermediates/Obj/%{cfg.buildcfg}")
    
    defines
    {
        "PRIMARY_GAME_MODULE",
    }

    files 
    {
        "Source/**.h",
        "Source/**.cpp",
        "%{wks.location}/Intermediates/Reflection/%{prj.name}/**.h",
        "%{wks.location}/Intermediates/Reflection/%{prj.name}/**.cpp",
    }

    includedirs 
    {
        "Source", -- game source
        path.join(LuminaEngineDirectory, "Lumina/Engine/Source"),
        path.join(LuminaEngineDirectory, "Lumina/Engine/Source/Runtime"),
        path.join(LuminaEngineDirectory, "Lumina/Engine/ThirdParty"),
        path.join(LuminaEngineDirectory, "Intermediates/Reflection/Lumina"),

        reflection_directory();
        includedependencies();
    }

    links 
    {
        path.join(LuminaEngineDirectory, "Binaries", outputdir, "Lumina")
    }

    prebuildcommands
    {
        "\"%{LuminaEngineDirectory}/Binaries/Release-windows-x86_64/Reflector.exe\" \"%{wks.location}%{wks.name}.sln\" && echo Reflection completed.",
        "python \"%{wks.location}GenerateProject.py\""
    }