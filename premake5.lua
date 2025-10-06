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
	}
		
	defines
	{
		"EASTL_USER_DEFINED_ALLOCATOR=1",
		"_CRT_SECURE_NO_WARNINGS",
		"GLM_FORCE_DEPTH_ZERO_TO_ONE",
		"IMGUI_DEFINE_MATH_OPERATORS",
	}

	filter "action:vs"
	
	filter "language:C++ or language:C"
		architecture "x86_64"

	buildoptions {
		"/Zm2000"
	}

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
        
        -- MSVC-specific Release optimizations
        filter { "configurations:Release", "system:windows" }
            buildoptions { 
                "/O2",          -- Maximum optimization (speed)
                "/Oi",          -- Enable intrinsic functions
                "/Ot",          -- Favor fast code
                "/GL",          -- Whole program optimization
                "/Gy",          -- Function-level linking
                "/fp:fast",     -- Fast floating point (careful with this!)
                "/arch:AVX2",   -- Use AVX2 instructions
            }
            linkoptions { 
                "/LTCG",        -- Link-time code generation
                "/OPT:REF",     -- Eliminate unreferenced functions
                "/OPT:ICF",     -- Identical COMDAT folding
            }

    -- Shipping Configuration (Maximum optimization, no symbols)
    filter "configurations:Shipping"
        runtime "Release"
        links { "%{VULKAN_SDK}/lib/shaderc_combined.lib" }
        optimize "Full"
        symbols "Off"
        defines { "LE_SHIP", "NDEBUG" }
        flags { "LinkTimeOptimization", "NoIncrementalLink", "NoRuntimeChecks" }
        
        -- MSVC-specific Shipping optimizations
        filter { "configurations:Shipping", "system:windows" }
            buildoptions { 
                "/O2",          -- Maximum optimization (speed)
                "/Oi",          -- Enable intrinsic functions
                "/Ot",          -- Favor fast code
                "/Ob3",         -- Aggressive inlining
                "/GL",          -- Whole program optimization
                "/Gy",          -- Function-level linking
                "/Gw",          -- Optimize global data
                "/fp:fast",     -- Fast floating point
                "/arch:AVX2",   -- Use AVX2 instructions
                "/GS-",         -- Disable security checks (use with caution!)
            }
            linkoptions { 
                "/LTCG",        -- Link-time code generation
                "/OPT:REF",     -- Eliminate unreferenced functions
                "/OPT:ICF",     -- Identical COMDAT folding
            }

    filter {}

	group "Dependencies"
		include "Lumina/Engine/ThirdParty/EA"
		include "Lumina/Engine/ThirdParty/EnkiTS"
		include "Lumina/Engine/ThirdParty/glfw"
		include "Lumina/Engine/ThirdParty/imgui"
		include "Lumina/Engine/Thirdparty/Tracy"
		include "Lumina/Engine/ThirdParty/xxhash"
		include "Lumina/Engine/ThirdParty/rpmalloc"
		include "Lumina/Engine/ThirdParty/entt"
	group ""

	group "Core"
		include "Lumina"
	group ""

	group "Applications"
		include "Lumina/Applications/LuminaEditor"
		include "Lumina/Applications/Reflector"
		include "Lumina/Applications/Sandbox"
	group ""