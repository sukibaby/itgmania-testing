# SoLoud
set(WITH_SOLOUD ON CACHE BOOL "Build with SoLoud audio library")
if(WITH_SOLOUD)
    set(SOLOUD_ROOT "${CMAKE_CURRENT_LIST_DIR}/soloud" CACHE PATH "Path to SoLoud root directory")
    if(NOT EXISTS "${SOLOUD_ROOT}")
        message(FATAL_ERROR "SoLoud root directory not found at ${SOLOUD_ROOT}")
    endif()

    set(SOLOUD_BACKEND_SDL2 TRUE CACHE BOOL "Use SDL2 backend for SoLoud")
    set(SOLOUD_BACKEND_WINMM TRUE CACHE BOOL "Use WinMM backend for SoLoud")
    set(SOLOUD_STATIC TRUE CACHE BOOL "Build static library")

    # Define the core source files
    set(SOLOUD_SOURCES "")
    
    # Core files - only what we need for basic 2D audio
    list(APPEND SOLOUD_SOURCES 
        "${SOLOUD_ROOT}/src/core/soloud.cpp"
        "${SOLOUD_ROOT}/src/core/soloud_audiosource.cpp"
        "${SOLOUD_ROOT}/src/core/soloud_bus.cpp"
        "${SOLOUD_ROOT}/src/core/soloud_core_basicops.cpp"
        "${SOLOUD_ROOT}/src/core/soloud_core_getters.cpp"
        "${SOLOUD_ROOT}/src/core/soloud_core_setters.cpp"
        "${SOLOUD_ROOT}/src/core/soloud_core_voiceops.cpp"
        "${SOLOUD_ROOT}/src/core/soloud_fader.cpp"
        "${SOLOUD_ROOT}/src/core/soloud_file.cpp"
        "${SOLOUD_ROOT}/src/core/soloud_thread.cpp"
    )
    
    foreach(source ${SOLOUD_SOURCES})
        if(NOT EXISTS "${source}")
            message(FATAL_ERROR "SoLoud source file not found: ${source}")
        endif()
    endforeach()

    # Audio source files
    set(SOLOUD_WAV_SOURCE "${SOLOUD_ROOT}/src/audiosource/wav/soloud_wav.cpp")
    if(EXISTS "${SOLOUD_WAV_SOURCE}")
        list(APPEND SOLOUD_SOURCES 
            "${SOLOUD_WAV_SOURCE}"
            "${SOLOUD_ROOT}/src/audiosource/wav/stb_vorbis.c"
        )
    else()
        message(FATAL_ERROR "Required WAV source file not found: ${SOLOUD_WAV_SOURCE}")
    endif()

    # Backend files
    set(SOLOUD_SDL2_SOURCE "${SOLOUD_ROOT}/src/backend/sdl2/soloud_sdl2.cpp")
    if(SDL2_FOUND AND EXISTS "${SOLOUD_SDL2_SOURCE}")
        list(APPEND SOLOUD_SOURCES "${SOLOUD_SDL2_SOURCE}")
        set(SOLOUD_HAS_SDL2 TRUE)
        message(STATUS "Using SDL2 backend for SoLoud")
    endif()

    set(SOLOUD_WINMM_SOURCE "${SOLOUD_ROOT}/src/backend/winmm/soloud_winmm.cpp")
    if(WIN32 AND EXISTS "${SOLOUD_WINMM_SOURCE}")
        list(APPEND SOLOUD_SOURCES "${SOLOUD_WINMM_SOURCE}")
        set(SOLOUD_HAS_WINMM TRUE)
        message(STATUS "Using WinMM backend for SoLoud")
    endif()

    # Create the library target
    add_library(soloud STATIC ${SOLOUD_SOURCES})

    target_include_directories(soloud PUBLIC
        "${SOLOUD_ROOT}/include"
    )

    if(NOT (SOLOUD_HAS_WINMM OR SOLOUD_HAS_SDL2))
        message(FATAL_ERROR "No audio backend available for SoLoud. Need either WinMM or SDL2.")
    endif()

    if(SOLOUD_HAS_WINMM)
        target_compile_definitions(soloud PRIVATE WITH_WINMM=1)
    endif()

    if(SOLOUD_HAS_SDL2)
        target_compile_definitions(soloud PRIVATE WITH_SDL2=1)
        target_link_libraries(soloud PRIVATE SDL2::SDL2)
    endif()

    set(SOLOUD_LIBRARY soloud CACHE INTERNAL "SoLoud library target")
endif()
