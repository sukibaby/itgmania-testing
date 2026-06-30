cmake_minimum_required(VERSION 3.15)
project(TextureFontGeneratorStandalone LANGUAGES CXX)

if(NOT MSVC)
  message(FATAL_ERROR "Texture Font Generator requires MSVC (Visual Studio) due to MFC dependency.")
endif()

include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("/D_AFXDLL" HAS_MFC_FLAG)

if(NOT HAS_MFC_FLAG)
    message(FATAL_ERROR "MFC support not found, Visual Studio MFC components are required to build Texture Font Generator.")
endif()

# ---------------------------------------------------------------------------
# Project-wide settings normally provided by the parent project.
# Adjust these if you fold this back into a larger build.
# ---------------------------------------------------------------------------
set(SM_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}" CACHE PATH "Root of the source tree containing 'Texture Font Generator'")
set(SM_GENERATED_SRC_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated" CACHE PATH "Directory for generated headers")
set(SM_PROGRAM_DIR "${CMAKE_CURRENT_BINARY_DIR}/bin" CACHE PATH "Output directory for built executables")

if(NOT COMMAND disable_project_warnings)
    function(disable_project_warnings target_name)
        # No-op fallback when not building inside the main project.
        # Replace with your own warning-suppression logic if desired.
    endfunction()
endif()

# ---------------------------------------------------------------------------
# stb_image / stb_image_write
# Prefer the existing CMake scripts if present; otherwise fall back to
# building minimal INTERFACE targets pointing at ../extern/stb/ headers.
# ---------------------------------------------------------------------------
set(STB_CMAKE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../extern")
set(STB_HEADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../extern/stb")

if(EXISTS "${STB_CMAKE_DIR}/CMakeProject-stb_image.cmake")
    include("${STB_CMAKE_DIR}/CMakeProject-stb_image.cmake")
else()
    message(STATUS "CMakeProject-stb_image.cmake not found, using header-only fallback from ${STB_HEADER_DIR}")
    if(NOT TARGET stb_image)
        add_library(stb_image INTERFACE)
        target_include_directories(stb_image INTERFACE "${STB_HEADER_DIR}")
    endif()
endif()

if(EXISTS "${STB_CMAKE_DIR}/CMakeProject-stb_image_write.cmake")
    include("${STB_CMAKE_DIR}/CMakeProject-stb_image_write.cmake")
else()
    message(STATUS "CMakeProject-stb_image_write.cmake not found, using header-only fallback from ${STB_HEADER_DIR}")
    if(NOT TARGET stb_image_write)
        add_library(stb_image_write INTERFACE)
        target_include_directories(stb_image_write INTERFACE "${STB_HEADER_DIR}")
    endif()
endif()

# ---------------------------------------------------------------------------
# Texture Font Generator target
# ---------------------------------------------------------------------------
set(TEXTURE_DIR "${SM_SRC_DIR}/Texture Font Generator")

set(TEXTURE_SRC
    "${TEXTURE_DIR}/stdafx.cpp"
    "${TEXTURE_DIR}/Texture Font Generator.cpp"
    "${TEXTURE_DIR}/Texture Font GeneratorDlg.cpp"
    "${TEXTURE_DIR}/TextureFont.cpp"
    "${TEXTURE_DIR}/Utils.cpp")

set(TEXTURE_HPP
    "${TEXTURE_DIR}/Resource.h"
    "${TEXTURE_DIR}/stdafx.h"
    "${TEXTURE_DIR}/Texture Font Generator.h"
    "${TEXTURE_DIR}/Texture Font GeneratorDlg.h"
    "${TEXTURE_DIR}/TextureFont.h"
    "${TEXTURE_DIR}/Utils.h")

set(TEXTURE_DAT
    "${TEXTURE_DIR}/res/Texture Font Generator.ico"
    "${TEXTURE_DIR}/Texture Font Generator.rc"
    "${TEXTURE_DIR}/res/Texture Font Generator.rc2")

source_group("Source Files" FILES ${TEXTURE_SRC} ${TEXTURE_HPP})
source_group("Resource Files" FILES ${TEXTURE_DAT})

set(CMAKE_MFC_FLAG 2)
add_executable("TextureFontGenerator"
               WIN32
               ${TEXTURE_SRC}
               ${TEXTURE_HPP}
               ${TEXTURE_DAT})
unset(CMAKE_MFC_FLAG)

set_property(TARGET "TextureFontGenerator" PROPERTY FOLDER "Internal Libraries")

disable_project_warnings("TextureFontGenerator")

target_link_libraries("TextureFontGenerator" PRIVATE stb_image_write)

set(TEXTURE_INCLUDE_DIRS
    "${TEXTURE_DIR}"
    "${TEXTURE_DIR}/res"
    "${SM_GENERATED_SRC_DIR}")

target_include_directories("TextureFontGenerator" PUBLIC ${TEXTURE_INCLUDE_DIRS})

set_target_properties("TextureFontGenerator"
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY                "${SM_PROGRAM_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE        "${SM_PROGRAM_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG          "${SM_PROGRAM_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL     "${SM_PROGRAM_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${SM_PROGRAM_DIR}"
        OUTPUT_NAME                             "Texture Font Generator"
        RELEASE_OUTPUT_NAME                     "Texture Font Generator"
        DEBUG_OUTPUT_NAME                       "Texture Font Generator"
        MINSIZEREL_OUTPUT_NAME                  "Texture Font Generator"
        RELWITHDEBINFO_OUTPUT_NAME              "Texture Font Generator")