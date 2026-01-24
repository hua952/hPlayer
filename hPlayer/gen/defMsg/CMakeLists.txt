SET(prjName defMsg)
SET(frameLibPath C:/work/firstBuildInstall/lib)

set(srcS)
set(genSrcS)
set(serFile)
set(pbFileS)
list(APPEND serFile C:/work/hPlayer/hPlayer/gen/defMsg/src/serialize/defMsgSer_com.cpp)
			
file(GLOB srcS src/*.cpp)
set(defS)
set(srcOS)
if (UNIX)
    MESSAGE(STATUS "unix")
elseif (WIN32)
	MESSAGE(STATUS "windows")
	ADD_DEFINITIONS(/W1)
	include_directories(
		${depIncludeHome}
		)
	link_directories(
		${depLibHome}
		)
endif ()
add_library(${prjName} ${genSrcS} ${srcS} ${srcOS} ${defS} ${pbFileS})
target_include_directories(${prjName} PRIVATE
src
							${firstBuildDir}/include/firstBuild
                            ${appFrameDir}/include/appFrame
							
						   )

SET(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
