/* RageSoundManager - Sound system interface using SoLoud backend.
 * 
 * This class provides a high-level interface for sound playback and management
 * using the SoLoud audio engine. It handles sound resource management, playback
 * control, and volume mixing.
 */
#ifndef RAGE_SOUND_MANAGER_H
#define RAGE_SOUND_MANAGER_H

#include <map>
#include "global.h"

namespace SoLoud {
    class Soloud;
    class AudioSource;
}

class RageSoundManager
{
public:
    RageSoundManager();
    ~RageSoundManager();

    /** Initialize the sound system. Must be called before any other sound operations. */
    void Init();
    
    /** Shutdown the sound system, stopping all sounds. */
    void Shutdown();
    
    /** Update the sound system. Call regularly to clean up unused resources. */
    void Update();

    /** Handle type for controlling individual sound instances */
    using SoundHandle = intptr_t;
    
    /** 
     * Play a sound once and forget about it.
     * @param path Path to the sound file
     * @param volume Playback volume (0.0 to 1.0)
     */
    void PlayOnce(const RString& path, float volume = 1.0f);
    
    /**
     * Play a sound and return a handle for controlling it.
     * @param path Path to the sound file
     * @param loop Whether to loop the sound
     * @param volume Initial volume (0.0 to 1.0)
     * @return Handle to control the sound, or 0 if failed
     */
    SoundHandle PlaySound(const RString& path, bool loop = false, float volume = 1.0f);
    
    /** Stop a playing sound */
    void StopSound(SoundHandle handle);
    
    /** Pause/unpause a sound */
    void PauseSound(SoundHandle handle, bool pause);
    
    /** Set the volume of a specific sound */
    void SetSoundVolume(SoundHandle handle, float volume);
    
    /** Set the playback speed of a sound (1.0 = normal) */
    void SetSoundSpeed(SoundHandle handle, float speed);
    
    /** Check if a sound is still playing */
    bool IsSoundPlaying(SoundHandle handle) const;
    
    /** Get the current playback position in seconds */
    float GetSoundPosition(SoundHandle handle) const;
    
    /** Stop all currently playing sounds */
    void StopAllSounds();
    
    /** Pause/unpause all sounds */
    void PauseAllSounds(bool pause);

    // Volume management
    /** Set the global mix volume based on preferences */
    void SetMixVolume();
    
    /** Set relative volume for non-critical sounds (e.g., background music) */
    void SetVolumeOfNonCriticalSounds(float vol);
    
    /** Get current volume scaling for non-critical sounds */
    float GetVolumeOfNonCriticalSounds() const { return m_fVolumeOfNonCriticalSounds; }

    /**
     * Preload a sound into memory.
     * Call this before playing to avoid loading delays.
     * @param path Path to the sound file
     * @return true if successfully loaded
     */
    bool PreloadSound(const RString& path);

    /** Get the audio system's sample rate */
    int GetSampleRate() const;
    
    /** Get the audio system's play latency in seconds */
    float GetPlayLatency() const;

    /** Get direct access to SoLoud engine (use with caution) */
    SoLoud::Soloud* GetSoLoud() const { return m_pSoLoud; }

private:
    /** Get or create a SoLoud audio source for the given path */
    SoLoud::AudioSource* GetSoLoudSource(const RString& path);

    SoLoud::Soloud* m_pSoLoud;                                    // Main SoLoud engine instance
    std::map<RString, SoLoud::AudioSource*> m_mapSoLoudSources;  // Cached sound sources
    float m_fVolumeOfNonCriticalSounds;                          // Volume scaling for background sounds

    // Prevent accidental copies
    RageSoundManager(const RageSoundManager& rhs) = delete;
    RageSoundManager& operator=(const RageSoundManager& rhs) = delete;
};

/** Global sound manager instance */
extern RageSoundManager* SOUNDMAN;

#endif