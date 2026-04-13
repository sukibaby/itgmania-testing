set(PNG_ROOT "${SM_EXTERN_DIR}/libpng")
set(PNG_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/libpng")
set(PNG_GENERATED_HPP "${PNG_GENERATED_DIR}/pnglibconf.h")

sm_glob_files(PNG_SRC
              BASE_DIR "${PNG_ROOT}"
              PATTERNS "*.c"
              EXCLUDE_PATTERNS "example.c")
sm_glob_files(PNG_SOURCE_HPP BASE_DIR "${PNG_ROOT}" PATTERNS "*.h")

configure_file("${PNG_ROOT}/scripts/pnglibconf.h.prebuilt"
               "${PNG_GENERATED_HPP}"
               COPYONLY)

set(PNG_HPP ${PNG_SOURCE_HPP} "${PNG_GENERATED_HPP}")

source_group(TREE "${PNG_ROOT}" FILES ${PNG_SRC} ${PNG_SOURCE_HPP})
source_group("Generated Files" FILES "${PNG_GENERATED_HPP}")

add_library("png" STATIC ${PNG_SRC} ${PNG_HPP})

set_property(TARGET "png" PROPERTY FOLDER "External Libraries")

disable_project_warnings("png")

# This wrapper only builds the generic libpng sources, so disable all
# architecture-specific SIMD entry points unless we migrate to upstream CMake.
target_compile_definitions("png" PRIVATE PNG_ARM_NEON_OPT=0
                                         PNG_INTEL_SSE_OPT=0
                                         PNG_POWERPC_VSX_OPT=0)

if(MSVC)
  target_compile_definitions("png" PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

target_include_directories("png" PUBLIC
  "${SM_EXTERN_DIR}/zlib"
  "${PNG_ROOT}"
  "${PNG_GENERATED_DIR}"
)
