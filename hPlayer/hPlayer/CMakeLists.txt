SET(prjName hPlayerModule)
project(${myProjectName} LANGUAGES C CXX)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)

# MSVC 配置
if(MSVC)
    add_compile_options(/utf-8 /W1 /MP)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

# 使用 vcpkg 的 SDL2
find_package(SDL2 CONFIG REQUIRED)

# FFmpeg 路径
set(FFMPEG_DIR "D:/myProgram/ffmpeg-output/windows" CACHE PATH "FFmpeg path")
#set(FFMPEG_DIR $ENV{FFMPEG_HOME} CACHE PATH "FFmpeg path")

#find_package(PkgConfig REQUIRED)
#pkg_check_modules(AVFORMAT REQUIRED libavformat)
#pkg_check_modules(AVCODEC REQUIRED libavcodec)
#pkg_check_modules(AVUTIL REQUIRED libavutil)
#pkg_check_modules(SWSCALE REQUIRED libswscale)
#pkg_check_modules(SWRESAMPLE REQUIRED libswresample)
#pkg_check_modules(AVDEVICE REQUIRED libavdevice)
#pkg_check_modules(AVFILTER REQUIRED libavfilter)

set(ffplaySrc)
file(GLOB ffplaySrc
    src/ffplay.c
    src/cmdutils.c
    src/opt_common.c
    src/ffplay_renderer.c
)
set(serSrcS)
set(genSrcS)
file(GLOB genSrcS src/userLogic/*.cpp src/*.cpp src/main/*.cpp src/main/userLogic/*.cpp src/main/playerData/*.cpp 
src/decoTh/*.cpp src/decoTh/userLogic/*.cpp src/decoTh/playerData/*.cpp 
src/videoDec/*.cpp src/videoDec/userLogic/*.cpp src/videoDec/playerData/*.cpp 
src/audioDec/*.cpp src/audioDec/userLogic/*.cpp src/audioDec/playerData/*.cpp 
)
set(defS)
set(libPath)
set(libDep)
set(osDepLib)
if (WIN32)
	MESSAGE(STATUS "windows")
	ADD_DEFINITIONS(/Zi)
	file(GLOB defS src/win/*.def)
	include_directories( )
endif ()

	include_directories(
	
    ${CMAKE_SOURCE_DIR}/gen/defMsg/src
	${CMAKE_SOURCE_DIR}/gen/hPlayerConfig/src
    src
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
${CMAKE_SOURCE_DIR}/gen/hPlayer/src/videoDec
src/videoDec
src/videoDec/userLogic
${CMAKE_SOURCE_DIR}/gen/hPlayer/src/audioDec
src/audioDec
src/audioDec/userLogic
)
list(APPEND libPath set(libDir ${firstBuildDir}/lib ${appFrameDir}/lib))
link_directories(${libPath} ${libDep})
	add_library(${prjName} SHARED ${genSrcS} ${defS} ${ffplaySrc})
if (UNIX)
	MESSAGE(STATUS "unix add -fPIC")
	find_package(unofficial-libuuid CONFIG REQUIRED)
	target_compile_options(${prjName} PRIVATE -fPIC)	
    set(VERSION_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/src/unix/lib.vers")
	target_link_options(${prjName} PRIVATE -Wl,-version-script,${VERSION_SCRIPT})
	list(APPEND osDepLib unofficial::UUID::uuid)
endif ()

# 头文件路径
target_include_directories(${prjName} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${FFMPEG_DIR}/include

    #${AVFORMAT_INCLUDE_DIRS}
    #${AVCODEC_INCLUDE_DIRS}
    #${AVUTIL_INCLUDE_DIRS}
    #${SWSCALE_INCLUDE_DIRS}
    #${SWRESAMPLE_INCLUDE_DIRS}
)

# 库文件路径
target_link_directories(${prjName} PRIVATE 
    ${FFMPEG_DIR}/lib

    #${AVFORMAT_LIBRARY_DIRS}
    #${AVCODEC_LIBRARY_DIRS}
    #${AVUTIL_LIBRARY_DIRS}
    #${SWSCALE_LIBRARY_DIRS}
    #${SWRESAMPLE_LIBRARY_DIRS}
)

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

avformat-62 avcodec-62 avutil-60
swscale-9 swresample-6 avfilter-11 avdevice-62

#avcodec
#avdevice
#avfilter
#avformat
#avutil
#swresample
#swscale

#${AVFORMAT_LIBRARIES}
#    ${AVCODEC_LIBRARIES}
#    ${AVUTIL_LIBRARIES}
#    ${SWSCALE_LIBRARIES}
#    ${SWRESAMPLE_LIBRARIES}
#    ${AVDEVICE_LIBRARIES}
#    ${AVFILTER_LIBRARIES}
    SDL2::SDL2 SDL2::SDL2main
	)
	SET(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
	install(TARGETS ${prjName} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
	
