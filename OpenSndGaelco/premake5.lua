project "OpenSndGaelco"
	targetname "OpenSndGaelco"
	language "C++"
	kind "SharedLib"
	removeplatforms { "x64" }

	files
	{
		"src/**.cpp", "src/**.h",
		"deps/cpp/**.cpp", "deps/inc/**.h",
		"src/OpenSndGaelco.aps", "src/OpenSndGaelco.rc"
	}

	includedirs { "src" }

postbuildcommands {
  "if not exist $(TargetDir)output mkdir $(TargetDir)output",
  "{COPY} $(TargetDir)OpenSndGaelco.dll $(TargetDir)output/"
}