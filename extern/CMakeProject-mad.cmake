# libmad 0.16+ configuration file for CMake.

# The generic 64-bit FPM should be used to avoid inline assembly.
set(FPM_64BIT 1)
configure_file("libmad/include/mad.h.in" "libmad/include/mad.h")
if(ENDIAN_BIG)
  set(WORDS_BIGENDIAN 1)
endif()

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
            "libmad/src/layer3.h")

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

target_include_directories("mad" PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/libmad/include")
target_include_directories("mad" PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/libmad/include")

# Use OPT_SPEED rather than OPT_ACCURACY to prevent in-game stutters caused by
# corrupted or non-compliant MP3 files. If OPT_ACCURACY is set, then libmad
# may briefly hold up decoding if an unexpected MP3 frame is encountered. We
# don't want libmad to be able to hold up the decoding process.
# Using either of the ASO_INTERLEAVE options is a significant improvement
# on in-game performance and CPU usage while decoding MP3 audio for any CPU
# which natively supports SSE or AVX instructions.
# Don't use ASO_ZEROCHECK - it negatively impacts clock accuracy, and does not
# provide any noticeable impact on in-game FPS or resource usage.
target_compile_definitions("mad" PRIVATE OPT_SPEED)
target_compile_definitions("mad" PRIVATE $<$<CONFIG:Debug>:DEBUG>)
target_compile_definitions("mad" PRIVATE ASO_INTERLEAVE2)

if(APPLE)
  # macOS builds fail on assert without including <stdlib.h> explicitly here.
  target_compile_options("mad" PRIVATE -include stdlib.h)
endif()

if(MSVC)
  target_compile_definitions("mad" PRIVATE _CRT_SECURE_NO_WARNINGS)
  target_compile_definitions("mad" PRIVATE inline=__inline)
  target_compile_options("mad" PRIVATE /O2)
else()
  target_compile_options("mad" PRIVATE -O3 -march=native)
endif()
