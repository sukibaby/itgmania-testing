#include "RageSoundReader_Resample_Good.h"

#include <samplerate.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "RageSoundReader.h"
#include "RageSoundReader_Filter.h"
#include "global.h"

/*
 * Resampling is done by libsamplerate
 * (https://github.com/libsndfile/libsamplerate).
 *
 * Differences from the previous polyphase implementation worth knowing about:
 *  - libsamplerate compensates for its own filter delay, so output frame 0 is
 *    aligned with input frame 0 and the tail is flushed at end of input.
 *  - Rate changes (SetRate) are exact; there is no 1/100 quantisation, so
 *    GetRate() returns exactly what was set.
 *  - Reads never use alloca(); arbitrarily large requests are safe.
 */

namespace {

// SRC_SINC_FASTEST / MEDIUM_QUALITY / BEST_QUALITY. MEDIUM is a good balance
// for real-time playback with 90% bandwidth but FASTEST is almost as good (80%
// bandwidth) and is the least CPU heavy as the name implies.
constexpr int kConverterType = SRC_SINC_FASTEST;

// Extra input requested beyond the strict minimum, so lookahead needs are met.
constexpr int kExtraInputFrames = 8;

}  // namespace

double RageSoundReader_Resample_Good::GetConversionRatio() const {
  const double rate = (rate_ == -1) ? 1.0 : static_cast<double>(rate_);
  return static_cast<double>(sample_rate_) /
         static_cast<double>(source_->GetSampleRate()) / rate;
}

/* Nothing has been run through the converter and nothing is buffered. */
bool RageSoundReader_Resample_Good::IsPristine() const {
  return pending_.empty() && consumed_input_frames_ == 0 && !source_eof_;
}

RageSoundReader_Resample_Good::RageSoundReader_Resample_Good(
    RageSoundReader* source, int sample_rate)
    : RageSoundReader_Filter(source),
      state_(nullptr),
      channels_(static_cast<int>(source->GetNumChannels())),
      sample_rate_(sample_rate),
      rate_(-1),
      source_eof_(false),
      consumed_input_frames_(0),
      emitted_input_time_(0) {
  int error = 0;
  state_ = src_new(kConverterType, channels_, &error);
  ASSERT(state_ != nullptr);
  src_set_ratio(state_, GetConversionRatio());
}

RageSoundReader_Resample_Good::RageSoundReader_Resample_Good(
    const RageSoundReader_Resample_Good& cpy)
    : RageSoundReader_Filter(cpy),
      state_(nullptr),
      channels_(cpy.channels_),
      sample_rate_(cpy.sample_rate_),
      rate_(cpy.rate_),
      pending_(cpy.pending_),
      source_eof_(cpy.source_eof_),
      consumed_input_frames_(cpy.consumed_input_frames_),
      emitted_input_time_(cpy.emitted_input_time_) {
  int error = 0;
  state_ = src_clone(cpy.state_, &error);  // copies the full filter state
  ASSERT(state_ != nullptr);
}

RageSoundReader_Resample_Good::~RageSoundReader_Resample_Good() {
  src_delete(state_);
}

RageSoundReader_Resample_Good* RageSoundReader_Resample_Good::Copy() const {
  return new RageSoundReader_Resample_Good(*this);
}

void RageSoundReader_Resample_Good::Reset() {
  src_reset(state_);
  src_set_ratio(state_, GetConversionRatio());
  pending_.clear();
  source_eof_ = false;
  consumed_input_frames_ = 0;
  emitted_input_time_ = 0;
}

/* frame is in the destination rate.  Seek the source in its own sample rate. */
int RageSoundReader_Resample_Good::SetPosition(int frame) {
  Reset();
  const int64_t source_frame =
      static_cast<int64_t>(frame) * source_->GetSampleRate() / sample_rate_;
  return source_->SetPosition(static_cast<int>(source_frame));
}

int RageSoundReader_Resample_Good::GetNextSourceFrame() const {
  const double buffered =
      static_cast<double>(pending_.size() / channels_) +
      std::max(0.0, consumed_input_frames_ - emitted_input_time_);
  double position =
      static_cast<double>(source_->GetNextSourceFrame()) - buffered;
  position = position * sample_rate_ / source_->GetSampleRate();
  return position < 0 ? 0 : static_cast<int>(position);
}

float RageSoundReader_Resample_Good::GetStreamToSourceRatio() const {
  float ratio = source_->GetStreamToSourceRatio();
  if (rate_ != -1) {
    ratio *= rate_;
  }
  return ratio;
}

bool RageSoundReader_Resample_Good::SetProperty(
    const std::string& property, float value) {
  if (property == "Rate") {
    SetRate(value);
    return true;
  }

  return source_->SetProperty(property, value);
}

/*
 * SetRate() changes the speed (and pitch) of the audio without changing the
 * sample rate: the stream consumes `ratio` source frames per output frame. The
 * new ratio is handed to libsamplerate on the next Read(), which ramps to it
 * smoothly across that block.
 */
void RageSoundReader_Resample_Good::SetRate(float ratio) {
  ASSERT(ratio > 0);
  rate_ = ratio;
  ASSERT(src_is_valid_ratio(GetConversionRatio()));
}

float RageSoundReader_Resample_Good::GetRate() const {
  return rate_ == -1 ? 1.0f : rate_;
}

int RageSoundReader_Resample_Good::Read(float* buf, int frames) {
  const double ratio = GetConversionRatio();

  /* If the ratio is 1:1 and nothing has been buffered, we're effectively
   * disabled, and can read directly into the caller's buffer. */
  if (ratio == 1.0 && IsPristine()) {
    return source_->Read(buf, frames);
  }

  // Top up the pending input so there is enough for `frames` of output.
  const int pending_frames = static_cast<int>(pending_.size() / channels_);
  const int wanted_input =
      static_cast<int>(std::ceil(frames / ratio)) + kExtraInputFrames;
  if (!source_eof_ && wanted_input > pending_frames) {
    const int to_read = wanted_input - pending_frames;
    const size_t old_size = pending_.size();
    pending_.resize(old_size + static_cast<size_t>(to_read) * channels_);

    int got = source_->Read(pending_.data() + old_size, to_read);
    if (got == RageSoundReader::END_OF_FILE) {
      source_eof_ = true;
      got = 0;
    } else if (got < 0) {
      pending_.resize(old_size);
      if (pending_frames == 0) {
        return got;  // error, WOULD_BLOCK, STREAM_LOOPED...
      }
      got = 0;  // still have buffered input to work with
    }
    pending_.resize(old_size + static_cast<size_t>(got) * channels_);
  }

  static const float kNoInput = 0.0f;
  SRC_DATA data;
  data.data_in = pending_.empty() ? &kNoInput : pending_.data();
  data.input_frames = static_cast<long>(pending_.size() / channels_);
  data.data_out = buf;
  data.output_frames = frames;
  data.input_frames_used = 0;
  data.output_frames_gen = 0;
  data.end_of_input = source_eof_ ? 1 : 0;
  data.src_ratio = ratio;

  if (src_process(state_, &data) != 0) {
    return RageSoundReader::ERROR;
  }

  // Drop the input libsamplerate has taken.
  pending_.erase(
      pending_.begin(),
      pending_.begin() +
          static_cast<size_t>(data.input_frames_used) * channels_);
  consumed_input_frames_ += static_cast<double>(data.input_frames_used);
  emitted_input_time_ += static_cast<double>(data.output_frames_gen) / ratio;

  if (data.output_frames_gen == 0 && source_eof_ && pending_.empty()) {
    return RageSoundReader::END_OF_FILE;
  }
  return static_cast<int>(data.output_frames_gen);
}

/*
 * (c) 2006 Glenn Maynard
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
