# Use the generic 64-bit FPM.
set(FPM_64BIT 1)

configure_file("libmad/include/mad.h.in" "libmad/include/mad.h")

set(MAD_SRC "libmad/src/bit.c"
            "libmad/src/decoder.c"
            "libmad/src/fixed.c"
            "libmad/src/frame.c"
            "libmad/src/huffman.c"
            "libmad/src/layer12.c"
            "libmad/src/layer3.c"
            "libmad/src/stream.c"
            "libmad/src/synth.c"
            "libmad/src/timer.c"
            "libmad/src/version.c")

set(MAD_HPP "libmad/include/mad.h"
            "libmad/src/global.h"
            "libmad/src/huffman.h"
            "libmad/src/layer12.h"
            "libmad/src/layer3.h"
            "libmad/src/config.h")

set(MAD_DAT "libmad/src/D.dat"
            "libmad/src/imdct_s.dat"
            "libmad/src/qc_table.dat"
            "libmad/src/rq_table.dat"
            "libmad/src/sf_table.dat")

source_group("Source Files" FILES ${MAD_SRC})
source_group("Header Files" FILES ${MAD_HPP})
source_group("Data Files" FILES ${MAD_DAT})

add_library("mad" STATIC ${MAD_SRC} ${MAD_HPP} ${MAD_DAT})

set_property(TARGET "mad" PROPERTY FOLDER "External Libraries")

disable_project_warnings("mad")

if(ENDIAN_BIG)
  set(WORDS_BIGENDIAN 1)
endif()

if(APPLE)
  target_compile_options("mad" PRIVATE -include stdlib.h)
endif()

target_compile_definitions("mad" PRIVATE $<$<CONFIG:Debug>:DEBUG>)
target_compile_definitions("mad" PRIVATE HAVE_CONFIG_H)

if(MSVC)
  target_compile_definitions("mad" PRIVATE _CRT_SECURE_NO_WARNINGS)
  target_compile_definitions("mad" PRIVATE ASO_ZEROCHECK)
  target_compile_definitions("mad" PRIVATE inline=__inline)
endif()

target_include_directories("mad" PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/libmad/include")
target_include_directories("mad" PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/libmad/include")

configure_file("config.mad.in.h" "libmad/src/config.h")
