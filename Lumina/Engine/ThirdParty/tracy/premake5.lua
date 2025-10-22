include(os.getenv("LUMINA_DIR") .. "/Dependencies.lua")


project "Tracy"
	kind "SharedLib"
	language "C++"

    targetdir ("%{wks.location}/Binaries/" .. outputdir)
    objdir ("%{wks.location}/Intermediates/Obj/" .. outputdir .. "/%{prj.name}")

    defines
    {
		"TRACY_ENABLE",
    	"TRACY_CALLSTACK",
    	"TRACY_ON_DEMAND",
		"TRACY_EXPORTS",
    }

	files
	{
		"public/TracyClient.cpp",
	}
	
	includedirs
	{
		"public",
	}