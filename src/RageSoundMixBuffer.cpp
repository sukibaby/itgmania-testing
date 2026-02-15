#include "RageSoundMixBuffer.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#ifdef __AVX__
#include <immintrin.h>
#endif

RageSoundMixBuffer::RageSoundMixBuffer() : m_iOffset(0) {}
RageSoundMixBuffer::~RageSoundMixBuffer() {}

/* write() will start mixing iOffset samples into the buffer.  Be careful; this
 * is measured in samples, not frames, so if the data is stereo, multiply by
 * two. */
void RageSoundMixBuffer::SetWriteOffset(int iOffset) {
  m_iOffset = static_cast<int>(iOffset);
}

void RageSoundMixBuffer::Extend(unsigned iSamples) {
  const unsigned realsize = iSamples + m_iOffset;
  if (m_pMixbuf.size() < realsize) {
    m_pMixbuf.resize(realsize, 0.0f);
  }
}

void RageSoundMixBuffer::write(
    const float* pBuf, unsigned iSize, int iSourceStride, int iDestStride) {
  if (iSize == 0) {
    return;
  }

  // iSize = 3, iDestStride = 2 uses 4 frames.  Don't allocate the stride of the
  // last sample.
  Extend(iSize * iDestStride - (iDestStride - 1));

  // Load the audio at m_iOffset into the buffer.
  float* pDestBuf = &m_pMixbuf[m_iOffset];

#ifdef __AVX__
  // AVX fast path for stride=1 (interleaved stereo mixing)
  if (iSourceStride == 1 && iDestStride == 1 && iSize >= 8) {
    unsigned iProcessed = 0;
    unsigned iVectorSize = (iSize / 8) * 8;

    for (unsigned i = 0; i < iVectorSize; i += 8) {
      __m256 vSrc = _mm256_loadu_ps(pBuf + i);
      __m256 vDest = _mm256_loadu_ps(pDestBuf + i);
      __m256 vResult = _mm256_add_ps(vDest, vSrc);
      _mm256_storeu_ps(pDestBuf + i, vResult);
    }

    pBuf += iVectorSize;
    pDestBuf += iVectorSize;
    iSize -= iVectorSize;
  }
#endif

  // Scalar fallback for remaining samples or non-unit strides
  while (iSize) {
    *pDestBuf += *pBuf;
    pBuf += iSourceStride;
    pDestBuf += iDestStride;
    --iSize;
  }
}

void RageSoundMixBuffer::read(int16_t* pBuf) {
#ifdef __AVX__
  // AVX fast path for bulk conversion and clamping
  const __m256 vMin = _mm256_set1_ps(-1.0f);
  const __m256 vMax = _mm256_set1_ps(1.0f);
  const __m256 vScale = _mm256_set1_ps(INT16_MAX);

  unsigned iSize = m_pMixbuf.size();
  unsigned iVectorSize = (iSize / 8) * 8;

  for (unsigned iPos = 0; iPos < iVectorSize; iPos += 8) {
    __m256 vIn = _mm256_loadu_ps(&m_pMixbuf[iPos]);
    // Clamp to [-1.0, 1.0]
    vIn = _mm256_max_ps(vIn, vMin);
    vIn = _mm256_min_ps(vIn, vMax);
    // Scale and convert to int32
    __m256 vScaled = _mm256_mul_ps(vIn, vScale);
    __m256i vInt32 = _mm256_cvttps_epi32(vScaled);
    // Pack int32 to int16 (takes lower 128 bits)
    __m128i vLower = _mm256_castsi256_si128(vInt32);
    __m128i vUpper = _mm256_extracti128_si256(vInt32, 1);
    __m128i vPacked = _mm_packs_epi32(vLower, vUpper);
    _mm_storeu_si128((__m128i*)&pBuf[iPos], vPacked);
  }

  // Scalar fallback for remaining samples
  for (unsigned iPos = iVectorSize; iPos < iSize; ++iPos) {
    float iOut = m_pMixbuf[iPos];
    iOut = std::clamp(iOut, -1.0f, +1.0f);
    pBuf[iPos] = static_cast<int16_t>(std::round(iOut * INT16_MAX));
  }
#else
  for (unsigned iPos = 0; iPos < m_pMixbuf.size(); ++iPos) {
    // do the read
    float iOut = m_pMixbuf[iPos];
    // ensure volume is within expected levels to prevent clipping
    iOut = std::clamp(iOut, -1.0f, +1.0f);
    // round rather than truncate to minimize distortion
    pBuf[iPos] = static_cast<int16_t>(std::round(iOut * INT16_MAX));
  }
#endif
  m_pMixbuf.clear();
}

void RageSoundMixBuffer::read(float* pBuf) {
#ifdef __AVX__
  // AVX fast path for bulk clamping
  const __m256 vMin = _mm256_set1_ps(-1.0f);
  const __m256 vMax = _mm256_set1_ps(1.0f);

  unsigned iSize = m_pMixbuf.size();
  unsigned iVectorSize = (iSize / 8) * 8;

  for (unsigned iPos = 0; iPos < iVectorSize; iPos += 8) {
    __m256 vIn = _mm256_loadu_ps(&m_pMixbuf[iPos]);
    // Clamp to [-1.0, 1.0]
    vIn = _mm256_max_ps(vIn, vMin);
    vIn = _mm256_min_ps(vIn, vMax);
    _mm256_storeu_ps(&pBuf[iPos], vIn);
  }

  // Scalar fallback for remaining samples
  for (unsigned iPos = iVectorSize; iPos < iSize; ++iPos) {
    pBuf[iPos] = std::clamp(m_pMixbuf[iPos], -1.0f, +1.0f);
  }
#else
  // ensure volume is within expected levels to prevent clipping
  std::transform(m_pMixbuf.begin(), m_pMixbuf.end(), pBuf, [](float s) {
    return std::clamp(s, -1.0f, +1.0f);
  });
#endif
  m_pMixbuf.clear();
}

void RageSoundMixBuffer::read_deinterlace(float** pBufs, int channels) {
  for (unsigned i = 0; i < m_pMixbuf.size() / channels; ++i) {
    for (int ch = 0; ch < channels; ++ch) {
      pBufs[ch][i] = m_pMixbuf[channels * i + ch];
    }
  }
  m_pMixbuf.clear();
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
