set(ZLIB_ROOT "${SM_EXTERN_DIR}/zlib")
set(ZLIB_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/zlib")
set(ZLIB_GENERATED_HPP "${ZLIB_GENERATED_DIR}/zconf.h")

sm_glob_files(ZLIB_SRC BASE_DIR "${ZLIB_ROOT}" PATTERNS "*.c")
sm_glob_files(ZLIB_SOURCE_HPP BASE_DIR "${ZLIB_ROOT}" PATTERNS "*.h")

configure_file("${ZLIB_ROOT}/zconf.h.in"
               "${ZLIB_GENERATED_HPP}"
               COPYONLY)

set(ZLIB_HPP ${ZLIB_SOURCE_HPP} "${ZLIB_GENERATED_HPP}")

source_group(TREE "${ZLIB_ROOT}" FILES ${ZLIB_SRC} ${ZLIB_SOURCE_HPP})
source_group("Generated Files" FILES "${ZLIB_GENERATED_HPP}")

add_library("zlib" STATIC ${ZLIB_SRC} ${ZLIB_HPP})

set_property(TARGET "zlib" PROPERTY FOLDER "External Libraries")

# Fixes clang build failures
# See itgmania/itgmania#107 for details
# TODO(teejusb/natano): Remove these two lines once these issues have been fixed upstream.
set_property(TARGET "zlib" PROPERTY C_STANDARD 90)
set_property(TARGET "zlib" PROPERTY C_STANDARD_REQUIRED ON)

disable_project_warnings("zlib")

if(MSVC)
  target_compile_definitions("zlib" PRIVATE _MBCS)
endif(MSVC)

target_include_directories("zlib" PUBLIC
  "${ZLIB_ROOT}"
  "${ZLIB_GENERATED_DIR}"
)
