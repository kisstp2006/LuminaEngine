
VULKAN_SDK = os.getenv("VULKAN_SDK")

project "ImGui"
	kind "StaticLib"
	language "C++"

    targetdir ("%{wks.location}/Binaries/" .. outputdir)
    objdir ("%{wks.location}/Intermediates/Obj/" .. outputdir .. "/%{prj.name}")

    defines
    {
        "LUMINA_ENGINE",
    }

	includedirs
	{
		".",
		"backends",
		"%{LuminaEngineDirectory}/Lumina/Engine/ThirdParty/glfw/include/",
		"%{VULKAN_SDK}/Include/",
	}

	files
	{
		"imconfig.h",
		"imgui.h",
		"implot.h",
        "implot.cpp",
        "implot_internal.h",
        "implot_items.cpp",
		"imgui.cpp",
		"imgui_draw.cpp",
		"imgui_internal.h",
		"imgui_widgets.cpp",
		"imstb_rectpack.h",
		"imstb_textedit.h",
		"imstb_truetype.h",
		"imgui_demo.cpp",
		"backends/imgui_impl_glfw.h",
		"backends/imgui_impl_glfw.cpp",
		"backends/imgui_impl_vulkan.h",
		"backends/imgui_impl_vulkan.cpp",
        "imgui_tables.cpp",
	}

	filter "system:windows"
		systemversion "latest"
		cppdialect "C++20"

	filter "system:linux"
		pic "On"
		systemversion "latest"
		cppdialect "C++20"

	filter "configurations:Debug"
		runtime "Debug"
		symbols "on"

	filter "configurations:Release"
		runtime "Release"
		optimize "on"
