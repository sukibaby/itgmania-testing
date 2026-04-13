set(MAD_ROOT "${SM_EXTERN_DIR}/libmad")
set(MAD_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/libmad")
set(MAD_GENERATED_HPP "${MAD_GENERATED_DIR}/config.h")

sm_glob_files(MAD_SRC
              BASE_DIR "${MAD_ROOT}"
              PATTERNS "*.c"
              EXCLUDE_PATTERNS "minimad.c")
sm_glob_files(MAD_SOURCE_HPP BASE_DIR "${MAD_ROOT}" PATTERNS "*.h")
sm_glob_files(MAD_DAT BASE_DIR "${MAD_ROOT}" PATTERNS "*.dat")

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/config.mad.in.h" "${MAD_GENERATED_HPP}")

set(MAD_HPP ${MAD_SOURCE_HPP} "${MAD_GENERATED_HPP}")

source_group(TREE "${MAD_ROOT}" FILES ${MAD_SRC} ${MAD_SOURCE_HPP} ${MAD_DAT})
source_group("Generated Files" FILES "${MAD_GENERATED_HPP}")

add_library("mad" STATIC ${MAD_SRC} ${MAD_HPP} ${MAD_DAT})

set_property(TARGET "mad" PROPERTY FOLDER "External Libraries")

disable_project_warnings("mad")

if(ENDIAN_BIG)
  set(WORDS_BIGENDIAN 1)
endif()

target_compile_definitions("mad" PRIVATE $<$<CONFIG:Debug>:DEBUG>)
target_compile_definitions("mad" PRIVATE HAVE_CONFIG_H)

if(MSVC)
  target_compile_definitions("mad" PRIVATE _CRT_SECURE_NO_WARNINGS)
  # TODO: Remove the need for this check since it's tied to 32-bit builds from
  # first glance.
  target_compile_definitions("mad" PRIVATE ASO_ZEROCHECK)
  target_compile_definitions("mad" PRIVATE $<$<CONFIG:Debug>:FPM_DEFAULT>)
  if(SM_WIN32_ARCH MATCHES "x64")
    target_compile_definitions("mad" PRIVATE $<$<CONFIG:Release>:FPM_64BIT>)
    target_compile_definitions("mad" PRIVATE $<$<CONFIG:MinSizeRel>:FPM_64BIT>)
    target_compile_definitions("mad" PRIVATE $<$<CONFIG:RelWithDebInfo>:FPM_64BIT>)
  else()
    target_compile_definitions("mad" PRIVATE $<$<CONFIG:Release>:FPM_INTEL>)
    target_compile_definitions("mad" PRIVATE $<$<CONFIG:MinSizeRel>:FPM_INTEL>)
    target_compile_definitions("mad" PRIVATE $<$<CONFIG:RelWithDebInfo>:FPM_INTEL>)
  endif()
  # TODO: Provide a proper define for inline.
  target_compile_definitions("mad" PRIVATE inline=__inline)
elseif(APPLE OR UNIX)
  target_compile_definitions("mad" PRIVATE FPM_64BIT=1)
endif(MSVC)

target_include_directories("mad" PRIVATE "${MAD_GENERATED_DIR}")
target_include_directories("mad" PUBLIC "${MAD_ROOT}")
