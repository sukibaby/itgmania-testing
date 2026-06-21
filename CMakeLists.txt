cmake_minimum_required(VERSION 3.20)

if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
  set(CMAKE_SYSTEM_VERSION 6.1 CACHE STRING "Target Windows version" FORCE)
  set(CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION 7.1 CACHE STRING "Target Windows SDK version" FORCE)
endif()

project(StepMania)

include(StepmaniaCore.cmake)

# The external libraries need to be included.
add_subdirectory(extern)

# The internal libraries and eventual executable to be used.
add_subdirectory(src)
