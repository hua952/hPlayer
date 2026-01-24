SET(prjName hPlayerConfig)
set(srcS)
file(GLOB srcS src/*.cpp)
if (UNIX)
	add_compile_options(-fPIC)
elseif (WIN32)
    MESSAGE(STATUS "windows")
	add_definitions(-DWIN32_LEAN_AND_MEAN)
	add_definitions(-w)
	ADD_DEFINITIONS(/Zi)
	ADD_DEFINITIONS(/W1)
endif ()

add_library(${prjName} ${srcS})
target_include_directories(${prjName} PRIVATE 
							${firstBuildDir}/include/firstBuild
                           )

SET(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

install(TARGETS ${prjName} ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

set(incS src/hPlayerConfig.h)

install(FILES ${incS} DESTINATION include/${myProjectName})