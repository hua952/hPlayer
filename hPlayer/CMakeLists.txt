cmake_minimum_required(VERSION 3.16) 
set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_HOME}/scripts/buildsystems/vcpkg.cmake")
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()
set(BUILD_USE_64BITS on)
set(CMAKE_CXX_STANDARD 20) 
set(CMAKE_CXX_STANDARD_REQUIRED True) 
if (UNIX)
	MESSAGE(STATUS "unix add -Wl,-Bsymbolic")
	set(CMAKE_CXX_LINK_OPTIONS "${CMAKE_CXX_LINK_OPTIONS} -Wl,-Bsymbolic")
endif ()

set (myProjectName hPlayer)
project(${myProjectName})

set(firstBuildDirDef ${CMAKE_SOURCE_DIR}/../../firstBuildInstall)

if(DEFINED ENV{firstBuildDir})
    set(firstBuildDirDef "$ENV{firstBuildDir}")
endif()

set(firstBuildDir "${firstBuildDirDef}" CACHE PATH "Path of firstBuild install")

message(STATUS "最终firstBuildDir是: ${firstBuildDir}")

set(appFrameDirDef ${CMAKE_SOURCE_DIR}/../../firstBuildInstall)

if(DEFINED ENV{appFrameDir})
    set(appFrameDirDef "$ENV{appFrameDir}")
endif()

set(appFrameDir "${appFrameDirDef}" CACHE PATH "Path of appFrame install")

message(STATUS "最终appFrameDir 是: ${appFrameDir}")

SET(CMAKE_INSTALL_PREFIX C:/work/hPlayer/hPlayerInstall)
add_subdirectory (gen)
	
add_subdirectory (hPlayer)
