set(SM_FFMPEG_SRC_DIR "${SM_EXTERN_DIR}/ffmpeg")
set(SM_FFMPEG_CONFIGURE_EXE "${SM_FFMPEG_SRC_DIR}/configure")
set(SM_FFMPEG_INSTALL_DIR "${CMAKE_BINARY_DIR}/ffmpeg-install")
set(SM_FFMPEG_LIB "${SM_FFMPEG_INSTALL_DIR}/lib")
set(SM_FFMPEG_INCLUDE "${SM_FFMPEG_INSTALL_DIR}/include")

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
            "--disable-shared"
            "--enable-zlib"
            "--prefix=${SM_FFMPEG_INSTALL_DIR}")

set(FFMPEG_CC "${CMAKE_C_COMPILER}")
set(FFMPEG_CXX "${CMAKE_CXX_COMPILER}")
if(CMAKE_C_COMPILER_LAUNCHER)
  string(REPLACE ";" " " FFMPEG_C_LAUNCHER "${CMAKE_C_COMPILER_LAUNCHER}")
  set(FFMPEG_CC "${FFMPEG_C_LAUNCHER} ${FFMPEG_CC}")
endif()
if(CMAKE_CXX_COMPILER_LAUNCHER)
  string(REPLACE ";" " " FFMPEG_CXX_LAUNCHER "${CMAKE_CXX_COMPILER_LAUNCHER}")
  set(FFMPEG_CXX "${FFMPEG_CXX_LAUNCHER} ${FFMPEG_CXX}")
endif()
list(APPEND FFMPEG_CONFIGURE "--cc=${FFMPEG_CC}" "--cxx=${FFMPEG_CXX}")

if(CMAKE_C_FLAGS)
  list(APPEND FFMPEG_CONFIGURE "--extra-cflags=${CMAKE_C_FLAGS}")
endif()
if(CMAKE_CXX_FLAGS)
  list(APPEND FFMPEG_CONFIGURE "--extra-cxxflags=${CMAKE_CXX_FLAGS}")
endif()

if(CMAKE_POSITION_INDEPENDENT_CODE)
  list(APPEND FFMPEG_CONFIGURE "--enable-pic")
endif()

if(WIN32)
  list(APPEND FFMPEG_CONFIGURE "--disable-asm")
  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    list(APPEND FFMPEG_CONFIGURE "--arch=x86_64")
  else()
    list(APPEND FFMPEG_CONFIGURE "--arch=x86")
  endif()
elseif(MACOSX)
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

set(SM_FFMPEG_MAKE_COMMAND)
if(MSVC)
  find_program(SM_FFMPEG_MAKE_PROGRAM NAMES nmake REQUIRED NO_CACHE)
  set(SM_FFMPEG_MAKE_COMMAND "${SM_FFMPEG_MAKE_PROGRAM}" "install")
else()
  if(CMAKE_GENERATOR STREQUAL "Xcode" OR CMAKE_GENERATOR MATCHES "Ninja")
    if(DEFINED ENV{CMAKE_BUILD_PARALLEL_LEVEL})
      set(NUM_CORES $ENV{CMAKE_BUILD_PARALLEL_LEVEL})
    else()
      cmake_host_system_information(RESULT NUM_CORES QUERY NUMBER_OF_LOGICAL_CORES)
    endif()
    set(SM_FFMPEG_MAKE_COMMAND "make" "-j${NUM_CORES}" "install")
  else()
    set(SM_FFMPEG_MAKE_COMMAND "make" "install")
  endif()
endif()

externalproject_add("ffmpeg"
                    SOURCE_DIR "${SM_FFMPEG_SRC_DIR}"
                    CONFIGURE_COMMAND ${FFMPEG_CONFIGURE}
                    BUILD_COMMAND ${SM_FFMPEG_MAKE_COMMAND}
                    UPDATE_COMMAND ""
                    INSTALL_COMMAND ""
                    TEST_COMMAND ""
                    BUILD_BYPRODUCTS
                      "${SM_FFMPEG_LIB}/libavformat.a"
                      "${SM_FFMPEG_LIB}/libavcodec.a"
                      "${SM_FFMPEG_LIB}/libswscale.a"
                      "${SM_FFMPEG_LIB}/libavutil.a"
                      "${SM_FFMPEG_LIB}/avformat.lib"
                      "${SM_FFMPEG_LIB}/avcodec.lib"
                      "${SM_FFMPEG_LIB}/swscale.lib"
                      "${SM_FFMPEG_LIB}/avutil.lib")
