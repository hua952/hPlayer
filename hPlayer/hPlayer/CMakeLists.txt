SET(prjName hPlayerModule)

find_package(PkgConfig REQUIRED)
pkg_check_modules(AVFORMAT REQUIRED libavformat)
pkg_check_modules(AVCODEC REQUIRED libavcodec)
pkg_check_modules(AVUTIL REQUIRED libavutil)
pkg_check_modules(SWSCALE REQUIRED libswscale)
pkg_check_modules(SWRESAMPLE REQUIRED libswresample)
pkg_check_modules(AVDEVICE REQUIRED libavdevice)
pkg_check_modules(AVFILTER REQUIRED libavfilter)


# SDL2 via pkg-config or find_package
pkg_check_modules(SDL2 REQUIRED sdl2)

set(VULKAN_DIR $ENV{VULKAN_SDK})
set(IMGUI_DIR $ENV{IMGUI_HOME})

set(serSrcS)
set(genSrcS)
file(GLOB genSrcS src/userLogic/*.cpp src/*.cpp src/main/*.cpp src/main/playerData/*.cpp src/main/userLogic/*.cpp src/decoTh/*.cpp src/decoTh/playerData/*.cpp src/decoTh/userLogic/*.cpp )
set(defS)
set(libPath)
set(libDep)
set(osDepLib)
if (WIN32)
	MESSAGE(STATUS "windows")
	ADD_DEFINITIONS(/Zi)
	ADD_DEFINITIONS(/W1)
	file(GLOB defS src/win/*.def)

	include_directories( )
endif ()
	include_directories(
	
    ${CMAKE_SOURCE_DIR}/gen/defMsg/src
	${CMAKE_SOURCE_DIR}/gen/hPlayerConfig/src
src/userLogic
${firstBuildDir}/include/firstBuild
    ${appFrameDir}/include/appFrame
    
${CMAKE_SOURCE_DIR}/gen/hPlayer/src
${CMAKE_SOURCE_DIR}/gen/hPlayer/src/main
src/main
src/main/userLogic
${CMAKE_SOURCE_DIR}/gen/hPlayer/src/decoTh
src/decoTh
src/decoTh/userLogic
)
list(APPEND libPath set(libDir ${firstBuildDir}/lib ${appFrameDir}/lib))
link_directories(${libPath} ${libDep})
include_directories(
    ${VULKAN_DIR}/Include
    ${AVFORMAT_INCLUDE_DIRS}
    ${AVCODEC_INCLUDE_DIRS}
    ${AVUTIL_INCLUDE_DIRS}
    ${SWSCALE_INCLUDE_DIRS}
    ${SWRESAMPLE_INCLUDE_DIRS}
    ${SDL2_INCLUDE_DIRS}
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

link_directories(
    ${VULKAN_DIR}/Lib
    ${AVFORMAT_LIBRARY_DIRS}
    ${AVCODEC_LIBRARY_DIRS}
    ${AVUTIL_LIBRARY_DIRS}
    ${SWSCALE_LIBRARY_DIRS}
    ${SWRESAMPLE_LIBRARY_DIRS}
    ${SDL2_LIBRARY_DIRS}
)

add_library(${prjName} SHARED 
    ${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp ${IMGUI_DIR}/imgui.cpp ${IMGUI_DIR}/imgui_draw.cpp ${IMGUI_DIR}/imgui_demo.cpp ${IMGUI_DIR}/imgui_tables.cpp ${IMGUI_DIR}/imgui_widgets.cpp
    ${genSrcS} ${defS})
if (UNIX)
	MESSAGE(STATUS "unix add -fPIC")
	find_package(unofficial-libuuid CONFIG REQUIRED)
	target_compile_options(${prjName} PRIVATE -fPIC)	
    set(VERSION_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/src/unix/lib.vers")
	target_link_options(${prjName} PRIVATE -Wl,-version-script,${VERSION_SCRIPT})
	list(APPEND osDepLib unofficial::UUID::uuid)
endif ()

target_compile_definitions(${prjName} PRIVATE SDL_MAIN_HANDLED)

target_link_libraries(${prjName} PRIVATE
	common
	hPlayerConfig
	hPlayerLib 
	logicCommon
	frameConfig
	cLog
	comTcpProNet
	${osDepLib}
defMsg

	vulkan-1
    shaderc_shared
    shaderc_util
    ${AVFORMAT_LIBRARIES}
    ${AVCODEC_LIBRARIES}
    ${AVUTIL_LIBRARIES}
    ${SWSCALE_LIBRARIES}
    ${SWRESAMPLE_LIBRARIES}
    ${AVDEVICE_LIBRARIES}
    ${AVFILTER_LIBRARIES}

    ${SDL2_LIBRARIES}
	)
	SET(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
	install(TARGETS ${prjName} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

