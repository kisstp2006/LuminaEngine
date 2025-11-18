include(os.getenv("LUMINA_DIR") .. "/Dependencies.lua")

project "Lumina"
    kind "SharedLib"
    language "C++"
    cppdialect "C++20"
    
    outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"
    
    targetdir ("%{LuminaEngineDirectory}/Binaries/" .. outputdir)
    objdir ("%{LuminaEngineDirectory}/Intermediates/Obj/" .. outputdir .. "/%{prj.name}")

    
    prebuildcommands
    {
        --"\"%{LuminaEngineDirectory}/Binaries/Release-windows-x86_64/Reflector.exe\" \"%{wks.location}%{wks.name}.sln\" && echo Reflection completed."
    }

    postbuildcommands
    {
        '{COPYFILE} "%{LuminaEngineDirectory}/External/PhysX/physx/bin/win.x86_64.vc143.mt/checked/*.dll" "%{cfg.targetdir}"',
        '{COPYFILE} "%{LuminaEngineDirectory}/External/RenderDoc/renderdoc.dll" "%{cfg.targetdir}"',
    }
    
    files
    {
        "Engine/Source/**.h",
        "Engine/Source/**.cpp",
        
        "%{LuminaEngineDirectory}/Intermediates/Reflection/Lumina/**.h",
        "%{LuminaEngineDirectory}/Intermediates/Reflection/Lumina/**.cpp",
        
        "Engine/ThirdParty/EnkiTS/src/**.h",
        "Engine/ThirdParty/EnkiTS/src/**.cpp",

        "Engine/ThirdParty/JoltPhysics/Jolt/**.cpp",
        "Engine/ThirdParty/JoltPhysics/Jolt/**.h",
        "Engine/ThirdParty/JoltPhysics/Jolt/**.inl",
        "Engine/ThirdParty/JoltPhysics/Jolt/**.gliffy",

        "Engine/ThirdParty/volk/**.h",
        "Engine/ThirdParty/entt/**.hpp",
        "Engine/ThirdParty/concurrentqueue/concurrentqueue.h",
        "Engine/ThirdParty/rpmalloc/**.h",
        "Engine/ThirdParty/rpmalloc/**.c",
        "Engine/ThirdParty/RenderDoc/renderdoc_app.h",
        "Engine/ThirdParty/stb_image/**.h",
        "Engine/ThirdParty/glm/glm/**.hpp",
        "Engine/ThirdParty/glm/glm/**.inl",
        "Engine/ThirdParty/imgui/imgui_demo.cpp",
        "Engine/ThirdParty/imgui/implot_demo.cpp",
        "Engine/ThirdParty/meshoptimizer/src/**.cpp",
        "Engine/ThirdParty/meshoptimizer/src/**.h",
        "Engine/ThirdParty/vk-bootstrap/src/**.h",
        "Engine/ThirdParty/vk-bootstrap/src/**.cpp",
        "Engine/ThirdParty/VulkanMemoryAllocator/include/**.h",
        "Engine/ThirdParty/json/include/**.h",
        "Engine/ThirdParty/json/src/**.cpp",
        "Engine/ThirdParty/ImGuizmo/**.h",
        "Engine/ThirdParty/ImGuizmo/**.cpp",
        "Engine/ThirdParty/SPIRV-Reflect/**.h",
        "Engine/ThirdParty/SPIRV-Reflect/**.c",
        "Engine/ThirdParty/SPIRV-Reflect/**.cpp",
        "Engine/ThirdParty/fastgltf/src/**.cpp",
        "Engine/ThirdParty/fastgltf/deps/simdjson/**.h",
        "Engine/ThirdParty/fastgltf/deps/simdjson/**.cpp",
    }

    includedirs
    { 
        "Engine/Source",
        "Engine/Source/Runtime",
        "Engine/ThirdParty/",
        "%{LuminaEngineDirectory}/Intermediates/Reflection/%{prj.name}",
        reflection_directory(),
        includedependencies(),
    }
    
    libdirs
    {
        
    }

    links
    {
        "GLFW",
        "ImGui",
        "EA",
        "Tracy",
        "XXHash",
    }
    


    defines
    {
        "LUMINA_ENGINE_DIRECTORY=%{LuminaEngineDirectory}",
        "LUMINA_ENGINE",

        "EASTL_USER_DEFINED_ALLOCATOR=1",

        "GLFW_INCLUDE_NONE",
        "GLFW_STATIC",

        "_SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS",

        "LUMINA_RENDERER_VULKAN",

        "TRACY_ENABLE",
        "TRACY_CALLSTACK",
        "TRACY_ON_DEMAND",
        "TRACY_IMPORTS",

        "LUMINA_RPMALLOC",

        "JPH_DEBUG_RENDERER",
        "JPH_FLOATING_POINT_EXCEPTIONS_ENABLED",
        "JPH_EXTERNAL_PROFILE",
		"JPH_ENABLE_ASSERTS",

        "IMGUI_IMPL_VULKAN_USE_VOLK",
    }
