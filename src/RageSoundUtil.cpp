#include "RageSoundUtil.h"

#include <algorithm>
#include <cstdint>

#include "RageUtil.h"

#ifdef __AVX__
#include <immintrin.h>
#endif

void RageSoundUtil::Attenuate(float* pBuf, int iSamples, float fVolume) {
#ifdef __AVX__
  // AVX fast path for volume multiplication
  if (iSamples >= 8) {
    __m256 vVolume = _mm256_set1_ps(fVolume);
    int iVectorSize = (iSamples / 8) * 8;
    
    for (int i = 0; i < iVectorSize; i += 8) {
      __m256 vSamples = _mm256_loadu_ps(&pBuf[i]);
      __m256 vResult = _mm256_mul_ps(vSamples, vVolume);
      _mm256_storeu_ps(&pBuf[i], vResult);
    }
    
    pBuf += iVectorSize;
    iSamples -= iVectorSize;
  }
#endif
  
  // Scalar fallback
  while (iSamples--) {
    *pBuf *= fVolume;
    ++pBuf;
  }
}

/* Pan buffer left or right; fPos is -1...+1.  Buffer is assumed to be stereo.
 */
void RageSoundUtil::Pan(float* buffer, int frames, float fPos) {
  if (fPos == 0) {
    return; /* no change */
  }

  bool bSwap = fPos < 0;
  if (bSwap) {
    fPos = -fPos;
  }

  float fLeftFactors[2] = {1 - fPos, 0};
  float fRightFactors[2] = {
      SCALE(fPos, 0, 1, 0.5f, 0), SCALE(fPos, 0, 1, 0.5f, 1)};

  if (bSwap) {
    std::swap(fLeftFactors[0], fRightFactors[0]);
    std::swap(fLeftFactors[1], fRightFactors[1]);
  }

#ifdef __AVX__
  // AVX fast path: process 4 stereo frames (8 samples) at a time
  if (frames >= 4) {
    __m256 vLeftL = _mm256_setr_ps(
        fLeftFactors[0], fLeftFactors[1], fLeftFactors[0], fLeftFactors[1],
        fLeftFactors[0], fLeftFactors[1], fLeftFactors[0], fLeftFactors[1]);
    __m256 vRightL = _mm256_setr_ps(
        fRightFactors[0], fRightFactors[1], fRightFactors[0], fRightFactors[1],
        fRightFactors[0], fRightFactors[1], fRightFactors[0], fRightFactors[1]);
    __m256 vRightR = _mm256_setr_ps(
        fRightFactors[0], fRightFactors[1], fRightFactors[0], fRightFactors[1],
        fRightFactors[0], fRightFactors[1], fRightFactors[0], fRightFactors[1]);
    __m256 vLeftR = _mm256_setr_ps(
        fLeftFactors[0], fLeftFactors[1], fLeftFactors[0], fLeftFactors[1],
        fLeftFactors[0], fLeftFactors[1], fLeftFactors[0], fLeftFactors[1]);
    
    int iVectorFrames = (frames / 4) * 4;
    for (int samp = 0; samp < iVectorFrames; samp += 4) {
      __m256 vIn = _mm256_loadu_ps(&buffer[samp * 2]);
      // Shuffle to separate L and R: [L0,R0,L1,R1,L2,R2,L3,R3]
      __m256 vL = _mm256_shuffle_ps(vIn, vIn, _MM_SHUFFLE(2, 0, 2, 0));
      __m256 vR = _mm256_shuffle_ps(vIn, vIn, _MM_SHUFFLE(3, 1, 3, 1));
      // Apply factors
      __m256 vOutL = _mm256_mul_ps(vL, vLeftL);
      __m256 vOutR = _mm256_mul_ps(vR, vRightL);
      __m256 vOut = _mm256_add_ps(vOutL, vOutR);
      _mm256_storeu_ps(&buffer[samp * 2], vOut);
    }
    
    buffer += iVectorFrames * 2;
    frames -= iVectorFrames;
  }
#endif

  // Scalar fallback
  for (int samp = 0; samp < frames; ++samp) {
    float l = buffer[0] * fLeftFactors[0] + buffer[1] * fLeftFactors[1];
    float r = buffer[0] * fRightFactors[0] + buffer[1] * fRightFactors[1];
    buffer[0] = l;
    buffer[1] = r;
    buffer += 2;
  }
}

void RageSoundUtil::Fade(
    float* pBuffer, int iFrames, int iChannels, float fStartVolume,
    float fEndVolume) {
  /* If the whole buffer is full volume, skip. */
  if (fStartVolume > .9999f && fEndVolume > .9999f) {
    return;
  }

  for (int iFrame = 0; iFrame < iFrames; ++iFrame) {
    float fVolPercent = SCALE(iFrame, 0, iFrames, fStartVolume, fEndVolume);

    fVolPercent = std::clamp(fVolPercent, 0.f, 1.f);
    for (int i = 0; i < iChannels; ++i) {
      *pBuffer *= fVolPercent;
      pBuffer++;
    }
  }
}

/* Dupe mono to stereo in-place; do it in reverse, to handle overlap. */
void RageSoundUtil::ConvertMonoToStereoInPlace(float* data, int iFrames) {
  float* input = data;
  float* output = input;
  input += iFrames;
  output += iFrames * 2;
  while (iFrames--) {
    input -= 1;
    output -= 2;
    output[0] = input[0];
    output[1] = input[0];
  }
}

void RageSoundUtil::ConvertNativeInt16ToFloat(
    const int16_t* pFrom, float* pTo, int iSamples) {
  for (int i = 0; i < iSamples; ++i) {
    pTo[i] = pFrom[i] / 32768.0f;
  }
}

void RageSoundUtil::ConvertFloatToNativeInt16(
    const float* pFrom, int16_t* pTo, int iSamples) {
  for (int i = 0; i < iSamples; ++i) {
    int iOut = static_cast<int>((pFrom[i] * 32768.0f) + 0.5);
    pTo[i] = std::clamp(iOut, -32768, 32767);
  }
}

/*
 * Copyright (c) 2002-2007 Glenn Maynard
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
