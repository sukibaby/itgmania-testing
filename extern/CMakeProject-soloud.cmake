# SoLoud
set(WITH_SOLOUD ON CACHE BOOL "Build with SoLoud audio library")
if(WITH_SOLOUD)
    set(SOLOUD_BACKEND_SDL2 TRUE CACHE BOOL "Use SDL2 backend for SoLoud")
    set(SOLOUD_BACKEND_WINMM TRUE CACHE BOOL "Use WinMM backend for SoLoud")
    set(SOLOUD_STATIC TRUE CACHE BOOL "Build static library")

    # Define the core source files
    set(SOLOUD_SOURCES "")
    
    # Core files
    list(APPEND SOLOUD_SOURCES 
        "${CMAKE_CURRENT_SOURCE_DIR}/soloud/src/core/soloud.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/soloud/src/core/soloud_audiosource.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/soloud/src/core/soloud_bus.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/soloud/src/core/soloud_core_3d.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/soloud/src/core/soloud_core_basicops.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/soloud/src/core/soloud_core_faderops.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/soloud/src/core/soloud_core_filterops.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/soloud/src/core/soloud_core_getters.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/soloud/src/core/soloud_core_setters.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/soloud/src/core/soloud_core_voicegroup.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/soloud/src/core/soloud_core_voiceops.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/soloud/src/core/soloud_fader.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/soloud/src/core/soloud_fft.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/soloud/src/core/soloud_fft_lut.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/soloud/src/core/soloud_file.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/soloud/src/core/soloud_filter.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/soloud/src/core/soloud_queue.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/soloud/src/core/soloud_thread.cpp"
    )

    # Audio source files
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/soloud/src/audiosource/wav/soloud_wav.cpp")
        list(APPEND SOLOUD_SOURCES 
            "${CMAKE_CURRENT_SOURCE_DIR}/soloud/src/audiosource/wav/soloud_wav.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/soloud/src/audiosource/wav/stb_vorbis.c"
        )
    endif()

    # Backend files
    if(SDL2_FOUND AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/soloud/src/backend/sdl2/soloud_sdl2.cpp")
        list(APPEND SOLOUD_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/soloud/src/backend/sdl2/soloud_sdl2.cpp")
        set(SOLOUD_HAS_SDL2 TRUE)
    endif()

    if(WIN32 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/soloud/src/backend/winmm/soloud_winmm.cpp")
        list(APPEND SOLOUD_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/soloud/src/backend/winmm/soloud_winmm.cpp")
        set(SOLOUD_HAS_WINMM TRUE)
    endif()

    # Create the library target
    add_library(soloud STATIC ${SOLOUD_SOURCES})

    target_include_directories(soloud PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/soloud/include"
    )

    if(SOLOUD_HAS_WINMM)
        target_compile_definitions(soloud PRIVATE WITH_WINMM=1)
    endif()

    if(SOLOUD_HAS_SDL2)
        target_compile_definitions(soloud PRIVATE WITH_SDL2=1)
        target_link_libraries(soloud PRIVATE SDL2::SDL2)
    endif()

    list(APPEND REQUIRED_LIBRARIES soloud)
endif()
