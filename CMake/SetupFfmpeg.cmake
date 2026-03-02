if(CMAKE_GENERATOR MATCHES "Ninja")
  message(
    FATAL_ERROR
      "You cannot use the Ninja generator when building the bundled ffmpeg library."
    )
endif()

set(SM_FFMPEG_SRC_DIR "${SM_EXTERN_DIR}/ffmpeg")
set(SM_FFMPEG_CONFIGURE_EXE "${SM_FFMPEG_SRC_DIR}/configure")
set(SM_FFMPEG_RELEASE_FILE "${SM_FFMPEG_SRC_DIR}/RELEASE")

set(SM_FFMPEG_VERSION "unknown")
if(EXISTS "${SM_FFMPEG_RELEASE_FILE}")
  file(READ "${SM_FFMPEG_RELEASE_FILE}" SM_FFMPEG_VERSION)
  string(STRIP "${SM_FFMPEG_VERSION}" SM_FFMPEG_VERSION)
endif()

get_filename_component(SM_FFMPEG_CACHE_PARENT "${SM_BUILD_DIR}" DIRECTORY)
set(SM_FFMPEG_CACHE_ROOT
  "${SM_FFMPEG_CACHE_PARENT}/ffmpeg-cache"
    CACHE PATH
          "Persistent directory used to cache bundled ffmpeg builds by version.")

set(SM_FFMPEG_CACHE_KEY
    "${SM_FFMPEG_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
if(MACOSX AND CMAKE_OSX_ARCHITECTURES)
  string(APPEND SM_FFMPEG_CACHE_KEY "-${CMAKE_OSX_ARCHITECTURES}")
endif()

if(CMAKE_POSITION_INDEPENDENT_CODE)
  string(APPEND SM_FFMPEG_CACHE_KEY "-pic")
endif()

if(NOT WITH_EXTERNAL_WARNINGS)
  string(APPEND SM_FFMPEG_CACHE_KEY "-nowarn")
endif()

string(REPLACE ";" "_" SM_FFMPEG_CACHE_KEY "${SM_FFMPEG_CACHE_KEY}")

set(SM_FFMPEG_CACHE_DIR "${SM_FFMPEG_CACHE_ROOT}/${SM_FFMPEG_CACHE_KEY}")
set(SM_FFMPEG_CACHE_BUILD_DIR "${SM_FFMPEG_CACHE_DIR}/build")
set(SM_FFMPEG_CACHE_INSTALL_DIR "${SM_FFMPEG_CACHE_DIR}/install")

set(SM_FFMPEG_LIB "${SM_FFMPEG_CACHE_INSTALL_DIR}/lib")
set(SM_FFMPEG_INCLUDE "${SM_FFMPEG_CACHE_INSTALL_DIR}/include")

set(SM_FFMPEG_CACHE_MARKER
    "${SM_FFMPEG_CACHE_INSTALL_DIR}/.itgmania-ffmpeg-${SM_FFMPEG_CACHE_KEY}.complete"
)

set(SM_FFMPEG_REQUIRED_ARTIFACTS
    "${SM_FFMPEG_LIB}/libavformat.a"
    "${SM_FFMPEG_LIB}/libavcodec.a"
    "${SM_FFMPEG_LIB}/libswscale.a"
    "${SM_FFMPEG_LIB}/libavutil.a"
    "${SM_FFMPEG_INCLUDE}/libavformat/avformat.h"
    "${SM_FFMPEG_CACHE_MARKER}")

set(SM_FFMPEG_CACHE_COMPLETE TRUE)
foreach(artifact IN LISTS SM_FFMPEG_REQUIRED_ARTIFACTS)
  if(NOT EXISTS "${artifact}")
    set(SM_FFMPEG_CACHE_COMPLETE FALSE)
    break()
  endif()
endforeach()

if(SM_FFMPEG_CACHE_COMPLETE)
  add_custom_target(
    "ffmpeg"
    COMMAND
      "${CMAKE_COMMAND}" -E echo
      "Using cached bundled ffmpeg (${SM_FFMPEG_VERSION}) from ${SM_FFMPEG_CACHE_INSTALL_DIR}"
    VERBATIM)
else()
  list(APPEND FFMPEG_CONFIGURE
              "${SM_FFMPEG_CONFIGURE_EXE}"
              "--disable-autodetect"
              "--disable-avdevice"
              "--disable-avfilter"
              "--disable-devices"
              "--disable-doc"
              "--disable-filters"
              "--disable-lzma"
              "--disable-network"
              "--disable-postproc"
              "--disable-programs"
              "--disable-swresample"
              "--disable-vaapi"
              "--disable-bzlib"
              "--enable-gpl"
              "--enable-version3"
              "--enable-pthreads"
              "--enable-static"
              "--enable-zlib"
              "--prefix=${SM_FFMPEG_CACHE_INSTALL_DIR}")

  if(CMAKE_POSITION_INDEPENDENT_CODE)
    list(APPEND FFMPEG_CONFIGURE "--enable-pic")
  endif()

  if(MACOSX)
    list(APPEND FFMPEG_CONFIGURE "--enable-cross-compile")
    list(APPEND FFMPEG_CONFIGURE "--enable-videotoolbox")
    list(APPEND FFMPEG_CONFIGURE "--extra-cflags=-mmacosx-version-min=11")
    if(CMAKE_OSX_ARCHITECTURES STREQUAL "arm64")
      list(APPEND FFMPEG_CONFIGURE "--arch=arm64" "--extra-cflags=-arch arm64" "--extra-ldflags=-arch arm64")
    elseif(CMAKE_OSX_ARCHITECTURES STREQUAL "x86_64")
      list(APPEND FFMPEG_CONFIGURE "--arch=x86_64" "--extra-cflags=-arch x86_64" "--extra-ldflags=-arch x86_64")
    else()
      message(FATAL_ERROR
        "Unsupported macOS architecture: ${CMAKE_OSX_ARCHITECTURES}, set CMAKE_OSX_ARCHITECTURES to either arm64 or x86_64"
      )
    endif()
  endif()

  if(NOT WITH_EXTERNAL_WARNINGS)
    list(APPEND FFMPEG_CONFIGURE "--extra-cflags=-w")
  endif()

  if(CMAKE_GENERATOR STREQUAL "Xcode")
    list(APPEND SM_FFMPEG_MAKE "make")
  else()
    list(APPEND SM_FFMPEG_MAKE $(MAKE))
  endif()

  if(WITH_FFMPEG_JOBS GREATER 0)
    list(APPEND SM_FFMPEG_MAKE "-j${WITH_FFMPEG_JOBS}")
  endif()
  list(APPEND SM_FFMPEG_MAKE
              "&&"
              "make"
              "install"
              "&&"
              "${CMAKE_COMMAND}"
              "-E"
              "touch"
              "${SM_FFMPEG_CACHE_MARKER}")

  externalproject_add("ffmpeg"
                      SOURCE_DIR "${SM_FFMPEG_SRC_DIR}"
                      BINARY_DIR "${SM_FFMPEG_CACHE_BUILD_DIR}"
                      CONFIGURE_COMMAND ${FFMPEG_CONFIGURE}
                      BUILD_COMMAND "${SM_FFMPEG_MAKE}"
                      UPDATE_COMMAND ""
                      INSTALL_COMMAND ""
                      TEST_COMMAND "")
endif()
