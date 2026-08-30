project "RmlOgre"
    kind "StaticLib"
    language "C++"
    cdialect "C17"
    cppdialect "C++23"
    toolset "v145"
    staticruntime "off"

	targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
	objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")

	files
	{
		"src/**.h",
		"src/**.hpp",
		"src/**.cpp"
	}

	includedirs
	{
		"src",
		ExtDir.."RmlUi/include",
		ExtDir.."ogre-next/include/OGRE-Next",
		ExtDir.."ogre-next/include/OGRE-Next/Hlms",
		ExtDir.."ogre-next/include/OGRE-Next/Hlms/Common"
	}

	links
	{
	}

	filter "system:windows"
		systemversion "latest"

	buildoptions
	{
        "/Zc:__cplusplus"
    }
