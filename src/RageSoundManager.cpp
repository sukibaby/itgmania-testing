/*
 * This manager has several distinct purposes:
 *
 * Load the sound driver, and handle most communication between it and RageSound.
 * Factory and reference count RageSoundReader objects for RageSound.
 * User-level:
 *  - global volume management
 *  - sound detaching ("play and delete when done playing")
 */

#include "global.h"
#include "RageSoundManager.h"
#include "RageUtil.h"
#include "RageLog.h"
#include "Preference.h"
#include "soloud.h"
#include "soloud_wav.h"

#include <cstdint>

static RageMutex g_SoundManMutex("SoundMan");

RageSoundManager *SOUNDMAN = nullptr;

RageSoundManager::RageSoundManager(): 
    m_pSoLoud(nullptr),
    m_fVolumeOfNonCriticalSounds(1.0f)
{}

void RageSoundManager::Init()
{
    m_pSoLoud = new SoLoud::Soloud;
    SoLoud::result result = m_pSoLoud->init(
        SoLoud::Soloud::CLIP_ROUNDOFF  // Use roundoff clipping for better audio quality
    );
    
    if (result != SoLoud::SO_NO_ERROR)
    {
        LOG->Warn("SoLoud initialization failed: %s", m_pSoLoud->getErrorString(result));
        delete m_pSoLoud;
        m_pSoLoud = nullptr;
        RageException::Throw("Failed to initialize SoLoud");
    }

    LOG->Info("SoLoud initialized successfully");
    // Set initial global volume
    m_pSoLoud->setGlobalVolume(g_fSoundVolume.Get());
}

RageSoundManager::~RageSoundManager()
{
    if (m_pSoLoud)
    {
        // Clean up audio sources first
        for (auto& source : m_mapSoLoudSources)
        {
            delete source.second;
        }
        m_mapSoLoudSources.clear();

        m_pSoLoud->deinit();
        delete m_pSoLoud;
        m_pSoLoud = nullptr;
    }
}

void RageSoundManager::Shutdown()
{
    if (m_pSoLoud)
    {
        m_pSoLoud->stopAll(); // Stop all playing sounds
    }
}

int64_t RageSoundManager::GetPosition(RageTimer* pTimer) const
{
    if (m_pSoLoud)
    {
        // Note: SoLoud uses seconds, convert to frames based on sample rate
        double timePos = m_pSoLoud->getStreamTime(0); // Use bus 0 (master)
        return static_cast<int64_t>(timePos * m_pSoLoud->getBackendSamplerate());
    }
    return 0;
}

void RageSoundManager::Update()
{
    if (!m_pSoLoud)
        return;

    // SoLoud handles its own updates internally
    // But we can use this to clean up stopped sounds if needed
    g_SoundManMutex.Lock();
    
    // Clean up any finished sounds
    for (auto it = m_mapSoLoudSources.begin(); it != m_mapSoLoudSources.end();)
    {
        // Check if this source has any active voices
        bool hasActiveVoices = false;
        for (unsigned int i = 0; i < m_pSoLoud->getActiveVoiceCount(); i++)
        {
            if (m_pSoLoud->getAudioSource(i) == it->second)
            {
                hasActiveVoices = true;
                break;
            }
        }

        if (!hasActiveVoices)
        {
            LOG->Trace("Cleaning up unused SoLoud source: %s", it->first.c_str());
            delete it->second;
            it = m_mapSoLoudSources.erase(it);
        }
        else
        {
            ++it;
        }
    }
    
    g_SoundManMutex.Unlock();
}

bool RageSoundManager::PreloadSound(const RString& path)
{
    if (!m_pSoLoud)
        return false;

    return GetSoLoudSource(path) != nullptr;
}

float RageSoundManager::GetPlayLatency() const
{
    return 0; // SoLoud handles latency internally
}

int RageSoundManager::GetSampleRate() const
{
    if (m_pSoLoud)
        return m_pSoLoud->getBackendSamplerate();
    return 48000;
}

SoLoud::AudioSource* RageSoundManager::GetSoLoudSource(const RString& sPath_)
{
    if (!m_pSoLoud)
        return nullptr;

    RString sPath(sPath_);
    sPath.MakeLower();

    auto it = m_mapSoLoudSources.find(sPath);
    if (it != m_mapSoLoudSources.end())
        return it->second;

    // Create new source
    auto* wav = new SoLoud::Wav();
    if (wav->load(sPath.c_str()) == SoLoud::SO_NO_ERROR)
    {
        m_mapSoLoudSources[sPath] = wav;
        return wav;
    }

    delete wav;
    return nullptr;
}

static Preference<float> g_fSoundVolume( "SoundVolume", 1.0f );

static Preference<float> g_fSoundVolume("SoundVolume", 1.0f);

void RageSoundManager::SetMixVolume()
{
    if (m_pSoLoud)
    {
        m_pSoLoud->setGlobalVolume(g_fSoundVolume.Get());
    }
}

void RageSoundManager::SetVolumeOfNonCriticalSounds(float fVolumeOfNonCriticalSounds)
{
    g_SoundManMutex.Lock();
    m_fVolumeOfNonCriticalSounds = fVolumeOfNonCriticalSounds;
    
    if (m_pSoLoud)
    {
        // In SoLoud we can set a separate bus for non-critical sounds
        // TODO: Implement bus system for non-critical sounds
        // For now we'll just scale the global volume
        m_pSoLoud->setGlobalVolume(g_fSoundVolume.Get() * fVolumeOfNonCriticalSounds);
    }
    
    g_SoundManMutex.Unlock();
}

// New SoLoud interface implementations
void RageSoundManager::PlayOnce(const RString& path, float volume)
{
    if (!m_pSoLoud)
        return;

    if (auto* source = GetSoLoudSource(path))
    {
        SoundHandle handle = m_pSoLoud->play(*source, volume);
        // No need to store the handle since this is fire-and-forget
    }
}

RageSoundManager::SoundHandle RageSoundManager::PlaySound(const RString& path, bool loop, float volume)
{
    if (!m_pSoLoud)
        return 0;

    if (auto* source = GetSoLoudSource(path))
    {
        source->setLooping(loop);
        return m_pSoLoud->play(*source, volume);
    }
    return 0;
}

void RageSoundManager::StopSound(SoundHandle handle)
{
    if (m_pSoLoud)
        m_pSoLoud->stop(handle);
}

void RageSoundManager::PauseSound(SoundHandle handle, bool pause)
{
    if (m_pSoLoud)
        m_pSoLoud->setPause(handle, pause);
}

void RageSoundManager::SetSoundVolume(SoundHandle handle, float volume)
{
    if (m_pSoLoud)
        m_pSoLoud->setVolume(handle, volume);
}

void RageSoundManager::SetSoundSpeed(SoundHandle handle, float speed)
{
    if (m_pSoLoud)
        m_pSoLoud->setRelativePlaySpeed(handle, speed);
}

bool RageSoundManager::IsSoundPlaying(SoundHandle handle) const
{
    if (!m_pSoLoud)
        return false;

    return m_pSoLoud->isValidVoiceHandle(handle);
}

float RageSoundManager::GetSoundPosition(SoundHandle handle) const
{
    if (!m_pSoLoud)
        return 0.0f;

    return m_pSoLoud->getStreamPosition(handle);
}

void RageSoundManager::StopAllSounds()
{
    if (m_pSoLoud)
        m_pSoLoud->stopAll();
}

void RageSoundManager::PauseAllSounds(bool pause)
{
    if (m_pSoLoud)
        m_pSoLoud->setPauseAll(pause);
}

/*
 * Copyright (c) 2002-2004 Glenn Maynard
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, and/or sell copies of the Software, and to permit persons to
 * whom the Software is furnished to do so, provided that the above
 * copyright notice(s) and this permission notice appear in all copies of
 * the Software and that both the above copyright notice(s) and this
 * permission notice appear in supporting documentation.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF
 * THIRD PARTY RIGHTS. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR HOLDERS
 * INCLUDED IN THIS NOTICE BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL INDIRECT
 * OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
