SET(prjName hPlayerLib)
set(serSrcS)
set(genSrcS)
file(GLOB genSrcS src/*.cpp src/main/*.cpp src/decoTh/*.cpp )
file(GLOB mainSrcS src/main.cpp )
set(defS)
set(libPath)
set(libDep)
set(mainLibS)
set(osSrc)
if (UNIX)
    MESSAGE(STATUS "unix")
	file(GLOB osSrc src/unix/*.cpp)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
elseif (WIN32)
	MESSAGE(STATUS "windows")
	ADD_DEFINITIONS(/Zi)
	ADD_DEFINITIONS(/W1)
	file(GLOB osSrc src/win/*.cpp)
	list(APPEND mainLibS ws2_32)
	include_directories( )
endif ()
	include_directories(
	src
    ${CMAKE_SOURCE_DIR}/gen/defMsg/src
	${CMAKE_SOURCE_DIR}/gen/hPlayerConfig/src
src/userLogic
${firstBuildDir}/include/firstBuild
        ${appFrameDir}/include/appFrame
    
src/main
src/decoTh
)
list(APPEND libPath set(libDir ${firstBuildDir}/lib) ${appFrameDir}/lib)
link_directories(${libPath} ${libDep})
	add_library(${prjName} ${genSrcS})
	add_executable (hPlayer ${mainSrcS} ${osSrc})
target_link_libraries(hPlayer PRIVATE
	common
	${mainLibS}
	)

	SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
	SET(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
	install(TARGETS ${prjName} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
	install(TARGETS hPlayer RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
	