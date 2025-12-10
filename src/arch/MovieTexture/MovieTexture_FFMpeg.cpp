#include "global.h"
#include "MovieTexture_FFMpeg.h"

#include "RageDisplay.h"
#include "RageLog.h"
#include "RageUtil.h"
#include "RageUtil/ConvertValue.h"
#include "RageFile.h"
#include "RageSurface.h"
#include "RageUtil/Endian.h"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <thread>

static void FixLilEndian()
{
	if constexpr (!Endian::little) {
		return;
	}

	static bool Initialized = false;
	if (Initialized)
		return;
	Initialized = true;

	for (int i = 0; i < AVPixelFormats[i].bpp; ++i)
	{
		AVPixelFormat_t& pf = AVPixelFormats[i];

		if (!pf.bByteSwapOnLittleEndian)
			continue;

		for (int mask = 0; mask < 4; ++mask)
		{
			int m = pf.masks[mask];
			switch (pf.bpp)
			{
			case 24: m = Swap24(m); break;
			case 32: m = Swap32(m); break;
			default:
				FAIL_M(ssprintf("Unsupported BPP value: %i", pf.bpp));
			}
			pf.masks[mask] = m;
		}
	}
}

static int FindCompatibleAVFormat(bool bHighColor)
{
	for (int i = 0; AVPixelFormats[i].bpp; ++i)
	{
		AVPixelFormat_t& fmt = AVPixelFormats[i];
		if (fmt.YUV != PixelFormatYCbCr_Invalid)
		{
			EffectMode em = MovieTexture_Generic::GetEffectMode(fmt.YUV);
			if (!DISPLAY->IsEffectModeSupported(em))
				continue;
		}
		else if (fmt.bHighColor != bHighColor)
		{
			continue;
		}

		RagePixelFormat pixfmt = DISPLAY->FindPixelFormat(fmt.bpp,
			fmt.masks[0],
			fmt.masks[1],
			fmt.masks[2],
			fmt.masks[3],
			true /* realtime */
		);

		if (pixfmt == RagePixelFormat_Invalid)
			continue;

		return i;
	}

	return -1;
}

RageSurface* RageMovieTextureDriver_FFMpeg::AVCodecCreateCompatibleSurface(int iTextureWidth, int iTextureHeight, bool bPreferHighColor, int& iAVTexfmt, MovieDecoderPixelFormatYCbCr& fmtout)
{
	FixLilEndian();

	int iAVTexfmtIndex = FindCompatibleAVFormat(bPreferHighColor);
	if (iAVTexfmtIndex == -1)
		iAVTexfmtIndex = FindCompatibleAVFormat(!bPreferHighColor);

	if (iAVTexfmtIndex == -1)
	{
		/* No dice.  Use the first avcodec format of the preferred bit depth,
		 * and let the display system convert. */
		for (iAVTexfmtIndex = 0; AVPixelFormats[iAVTexfmtIndex].bpp; ++iAVTexfmtIndex)
			if (AVPixelFormats[iAVTexfmtIndex].bHighColor == bPreferHighColor)
				break;
		ASSERT(AVPixelFormats[iAVTexfmtIndex].bpp != 0);
	}

	const AVPixelFormat_t* pfd = &AVPixelFormats[iAVTexfmtIndex];
	iAVTexfmt = pfd->pf;
	fmtout = pfd->YUV;

	LOG->Trace("Texture pixel format: %i %i (%ibpp, %08x %08x %08x %08x)", iAVTexfmt, fmtout,
		pfd->bpp, pfd->masks[0], pfd->masks[1], pfd->masks[2], pfd->masks[3]);

	if (pfd->YUV == PixelFormatYCbCr_YUYV422)
		iTextureWidth /= 2;

	return CreateSurface(iTextureWidth, iTextureHeight, pfd->bpp,
		pfd->masks[0], pfd->masks[1], pfd->masks[2], pfd->masks[3]);
}

MovieDecoder_FFMpeg::MovieDecoder_FFMpeg()
{
	FixLilEndian();

	av_format_context_ = nullptr;
	av_stream_ = nullptr;
	av_pixel_format_ = avcodec::AV_PIX_FMT_BGRA; // Default RGB target; may be overwritten by surface setup.
	total_frames_ = 0;
	end_of_file_ = 0;
	// Hardcoded frame buffer size of 50. Roughly translates to 100mb of ram.
	for (int i = 0; i < 50; i++) {
		frame_buffer_.emplace_back(std::make_unique<FrameHolder>());
	}
}

MovieDecoder_FFMpeg::~MovieDecoder_FFMpeg()
{
	if (av_sws_context_)
	{
		avcodec::sws_freeContext(av_sws_context_);
		av_sws_context_ = nullptr;
	}
	if (rgb_frame_ != nullptr) {
		avcodec::av_frame_free(&rgb_frame_);
	}
	if (av_io_context_ != nullptr)
	{
		RageFile* file = (RageFile*)av_io_context_->opaque;
		file->Close();
		delete file;
		avcodec::av_free(av_io_context_);
	}
	if (av_buffer_ != nullptr)
	{
		avcodec::av_free(av_buffer_);
	}
	if (av_stream_codec_ != nullptr)
	{
		avcodec::avcodec_free_context(&av_stream_codec_);
	}
	packet_buffer_.clear();
	frame_buffer_.clear();
}

void MovieDecoder_FFMpeg::Init()
{
	end_of_file_ = 0;
	display_frame_num_ = 0;
	if (av_sws_context_ != nullptr)
	{
		avcodec::sws_freeContext(av_sws_context_);
	}
	av_sws_context_ = nullptr;
	if (rgb_frame_ != nullptr) {
		avcodec::av_frame_free(&rgb_frame_);
	}
	rgb_frame_ = nullptr;
	sws_width_ = 0;
	sws_height_ = 0;
	av_io_context_ = nullptr;
	av_buffer_ = nullptr;
}

float MovieDecoder_FFMpeg::GetTimestamp() const
{
	// Always display the first frame.
	if (display_frame_num_ == 0) {
		return 0;
	}

	// In a logical situation, this means that display is outpacing decoding.
	if (display_frame_num_ >= packet_buffer_.size()) {
		return 0;
	}

	PacketHolder* packet = packet_buffer_[display_frame_num_].get();

	// Sanity check.
	if (packet == nullptr) {
		return 0;
	}

	std::lock_guard<std::mutex> lock(packet->lock);
	return packet->frame_timestamp - timestamp_offset_;
}

bool MovieDecoder_FFMpeg::IsCurrentFrameReady() {
	// We're displaying faster than decoding. Do not even try to display the
	// frame.
	if (display_frame_num_ >= packet_buffer_.size()) {
		return false;
	}

	FrameHolder* frame = frame_buffer_[GetFrameBufferIndex(display_frame_num_)].get();
	// To make sure the frame doesn't change from under us.
	std::lock_guard<std::mutex> lock(frame->lock);

	// In terms of how the sliding window works, displayed and ready are
	// opposites. If the frame hasn't been displayed, it's ready. If it has
	// been displayed, then it hasn't been overwritten and reset yet.
	if (frame->displayed) {
		LOG->Info("Frame %zu not decoded, total frames: %zu", display_frame_num_, total_frames_);
	}
	return !frame->displayed;
}

int MovieDecoder_FFMpeg::HandleNextPacket() {
	// If the decoder hit the end of the file, then that means the packet
	// buffer is complete.
	if (!end_of_file_) {
		// Add in a new FrameBuffer entry, and lock it immediately.
		packet_buffer_.emplace_back(std::make_unique<PacketHolder>());
		std::unique_lock<std::mutex> lock(packet_buffer_.back()->lock);
		int status = SendPacketToBuffer();
		if (status < 0) {
			lock.unlock();
			return status;
		}

		// If the decoded packet is the end of file.
		if (end_of_file_) {
			// Release the mutex.
			lock.unlock();

			packet_buffer_.pop_back(); // Don't display an EoF frame.
			// If we had to approximate the number of frames, set the actual
			// total number of frames. This is benign even if we did have an
			// accurate frame count at the start.
			total_frames_ = packet_buffer_.size();
			if (total_frames_ < frame_buffer_.size()) {
				LOG->Trace("Video shorter than frame buffer, shrinking the buffer.");
				frame_buffer_.resize(total_frames_);
			}
			return 1;
		}
		lock.unlock();
	}
	return 0;
}

int MovieDecoder_FFMpeg::DecodeFrame()
{
	int status = HandleNextPacket();
	if (status != 0) {
		return status;
	}

	status = DecodePacketToFrame();
	frame_buffer_position_ = (frame_buffer_position_ + 1) % frame_buffer_.size();
	packet_buffer_position_ = (packet_buffer_position_ + 1) % total_frames_;
	return status;
}

void MovieDecoder_FFMpeg::HandleReset() {
	reset_ = false;

	for (std::unique_ptr<FrameHolder>& frame : frame_buffer_) {
		frame->displayed = true;
	}
	// If not end of file, reset the decoding.
	if (!end_of_file_) {
		avcodec::av_seek_frame(av_format_context_, -1, 0, 0);
		OpenCodec();
		packet_buffer_.clear();
	}
	offset_ = 0;
	next_offset_ = 0;
	display_frame_num_ = 0;
	packet_buffer_position_ = 0;
	frame_buffer_position_ = 0;
}

int MovieDecoder_FFMpeg::DecodeMovie()
{
	// Never exit when the movie is looping. Otherwise exit when the last frame
	// is added to the FrameBuffer.
	while (looping_ || (!looping_ && display_frame_num_ < total_frames_)) {
		if (reset_) {
			HandleReset();
		}

		int status = DecodeFrame();

		// If cancelled (quitting a song, scrolling the banner), or fatal error,
		// stop decoding.
		if (status < 0) {
			return status;
		}

		// This means when opening the file, less frames were detected than
		// there actually are. Increment to keep up so we don't end the video
		// early during display.
		if (packet_buffer_position_ >= total_frames_ - 1 && !end_of_file_) {
			total_frames_++;
		}
	}

	return 0;
}

int MovieDecoder_FFMpeg::SendPacketToBuffer()
{
	if (cancel_) {
		return -2;
	}
	if (end_of_file_ > 0) {
		return 0;
	}

	while (true)
	{
		int ret = avcodec::av_read_frame(av_format_context_, packet_buffer_.back()->packet);
		/* XXX: why is avformat returning AVERROR_NOMEM on EOF? */
		if (ret < 0)
		{
			end_of_file_ = 1;
			return 0;
		}

		if (packet_buffer_.back()->packet->stream_index == av_stream_->index)
		{
			return 1;
		}
		/* It's not for the video stream; ignore it. */
		avcodec::av_packet_unref(packet_buffer_.back()->packet);
	}
}

bool MovieDecoder_FFMpeg::PrepareFrameSlot(FrameHolder* frame) {
	if (packet_buffer_.size() <= frame_buffer_.size()) {
		return true;
	}

	while (!frame->displayed) {
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
		if (cancel_) {
			return false;
		}
		if (reset_) {
			return false;
		}
	}

	return true;
}

void MovieDecoder_FFMpeg::UpdateFrameTiming(PacketHolder* packet, FrameHolder* frame) {
	if (frame->frame->pkt_dts != AV_NOPTS_VALUE)
	{
		packet->frame_timestamp = static_cast<float>(frame->frame->pkt_dts * av_q2d(av_stream_->time_base));
	}
	else
	{
		if (packet_buffer_position_ != 0) {
			packet->frame_timestamp += packet_buffer_[packet_buffer_.size() - 2]->frame_delay;
		}
		else {
			packet->frame_timestamp = 0;
		}
	}

	if (packet_buffer_position_ == 0 && packet->frame_timestamp != 0) {
		timestamp_offset_ = packet->frame_timestamp;
	}

	packet->frame_delay = static_cast<float>(av_q2d(av_stream_->time_base));
	packet->frame_delay += frame->frame->repeat_pict * (packet->frame_delay * 0.5f);
}

int MovieDecoder_FFMpeg::DecodePacketIntoFrame(PacketHolder* packet, FrameHolder* frame) {
	std::lock_guard<std::mutex> frame_lock(frame->lock);
	std::lock_guard<std::mutex> packet_lock(packet->lock);

	if (packet->decoded && packet_buffer_position_ == 0) {
		next_offset_ = frame_buffer_position_;
	}

	int packet_offset = 0;
	while (packet_offset <= packet->packet->size)
	{
		if (packet->packet->size == 0 && packet_buffer_position_ == 0) {
			return 0; /* eof */
		}

		packet->packet->data = packet->packet->size ? packet->packet->data : nullptr;
		const int len = packet->packet->size;
		avcodec::avcodec_send_packet(av_stream_codec_, packet->packet);
		int avcodec_return = avcodec::avcodec_receive_frame(av_stream_codec_, frame_buffer_[frame_buffer_position_]->frame);
		frame->displayed = false;
		frame->packet_num = packet_buffer_position_;

		if (len < 0)
		{
			LOG->Warn("avcodec_decode_video2 fatal error, packet size negative: %i", len);
			return -1;
		}

		packet_offset += len;

		if (avcodec_return != 0)
		{
			LOG->Trace(
				"Frame %i saw nonzero avcodec_receive_frame status: %i, this is likely not fatal.",
				static_cast<int>(packet_buffer_.size() - 1),
				avcodec_return);

			if (packet_offset <= packet->packet->size) {
				continue;
			}
		}

		UpdateFrameTiming(packet, frame);
		packet->decoded = true;
		return 1;
	}

	return 0; /* packet done */
}

int MovieDecoder_FFMpeg::DecodePacketToFrame() {
	if (cancel_) {
		return -2;
	}

	frame_buffer_position_ %= frame_buffer_.size();
	packet_buffer_position_ %= total_frames_;
	FrameHolder* frame = frame_buffer_[frame_buffer_position_].get();
	PacketHolder* packet = packet_buffer_[packet_buffer_position_].get();

	if (!PrepareFrameSlot(frame)) {
		return cancel_ ? -2 : 0;
	}

	return DecodePacketIntoFrame(packet, frame);
}

bool MovieDecoder_FFMpeg::EnsureSwsContext()
{
	if (av_sws_context_ != nullptr) {
		return true;
	}

	sws_width_ = sws_width_ == 0 ? GetWidth() : sws_width_;
	sws_height_ = sws_height_ == 0 ? GetHeight() : sws_height_;

	av_sws_context_ = avcodec::sws_getCachedContext(av_sws_context_,
		sws_width_, sws_height_, av_stream_codec_->pix_fmt,
		sws_width_, sws_height_, av_pixel_format_,
		kSwsFlags, nullptr, nullptr, nullptr);
	if (av_sws_context_ == nullptr)
	{
		LOG->Warn("Cannot initialize sws conversion context for (%d,%d) %d->%d", sws_width_, sws_height_, av_stream_codec_->pix_fmt, av_pixel_format_);
		return false;
	}

	return true;
}

int MovieDecoder_FFMpeg::BlitFrameToSurface(FrameHolder* frame, RageSurface* surface_out)
{
	if (rgb_frame_ == nullptr) {
		rgb_frame_ = avcodec::av_frame_alloc();
	}

	rgb_frame_->data[0] = static_cast<unsigned char*>(surface_out->pixels);
	rgb_frame_->linesize[0] = surface_out->pitch;

	if (!EnsureSwsContext()) {
		return -1;
	}

	if (frame->packet_num == display_frame_num_) {
		return avcodec::sws_scale(av_sws_context_,
			frame->frame->data, frame->frame->linesize, 0, sws_height_,
			rgb_frame_->data, rgb_frame_->linesize);
	}

	LOG->Warn("Unexpected frame trying to display! display_frame_num_ = %zu, packet_num = %zu", display_frame_num_, frame->packet_num);
	return 0;
}

int MovieDecoder_FFMpeg::GetFrame(RageSurface* surface_out)
{
	const size_t display_frame_in_buffer = GetFrameBufferIndex(display_frame_num_);
	std::lock_guard<std::mutex> lock(frame_buffer_[display_frame_in_buffer]->lock);

	int scale_status = BlitFrameToSurface(frame_buffer_[display_frame_in_buffer].get(), surface_out);
	frame_buffer_[display_frame_in_buffer]->displayed = true;

	if (LastFrame()) {
		end_of_movie_ = true;
		return scale_status;
	}
	end_of_movie_ = false;
	display_frame_num_++;
	return scale_status;
}

static RString averr_ssprintf(int err, const char* fmt, ...)
{
	ASSERT(err < 0);

	va_list     va;
	va_start(va, fmt);
	RString s = vssprintf(fmt, va);
	va_end(va);

	size_t errbuf_size = 512;
	char* errbuf = new char[errbuf_size];
	avcodec::av_strerror(err, errbuf, errbuf_size);
	RString Error = ssprintf("%i: %s", err, errbuf);
	delete[] errbuf;

	return s + " (" + Error + ")";
}

static int AVIORageFile_ReadPacket(void* opaque, uint8_t* buf, int buf_size)
{
	RageFile* f = (RageFile*)opaque;
	int n = f->Read(buf, buf_size);
	if (n == 0)
		return AVERROR_EOF;
	return n;
}

static int64_t AVIORageFile_Seek(void* opaque, int64_t offset, int whence)
{
	RageFile* f = (RageFile*)opaque;
	if (whence == AVSEEK_SIZE)
		return f->GetFileSize();

	if (whence != SEEK_SET && whence != SEEK_CUR && whence != SEEK_END)
	{
		LOG->Trace("Error: unsupported seek whence: %d", whence);
		return -1;
	}

	return f->Seek((int)offset, whence);
}

RString MovieDecoder_FFMpeg::Open(RString file)
{
	av_format_context_ = avcodec::avformat_alloc_context();
	if (!av_format_context_)
		return "AVCodec: Couldn't allocate context";

	RageFile* f = new RageFile;

	if (!f->Open(file, RageFile::READ))
	{
		RString errorMessage = f->GetError();
		RString error = ssprintf("MovieDecoder_FFMpeg: Error opening \"%s\": %s", file.c_str(), errorMessage.c_str());
		delete f;
		return error;
	}

	av_buffer_ = static_cast<unsigned char*>(avcodec::av_malloc(kFFMpegBufferSize));
	av_io_context_ = avcodec::avio_alloc_context(av_buffer_, kFFMpegBufferSize, 0, f, AVIORageFile_ReadPacket, nullptr, AVIORageFile_Seek);

	av_format_context_->pb = av_io_context_;
	int ret = avcodec::avformat_open_input(&av_format_context_, file.c_str(), nullptr, nullptr);
	if (ret < 0)
		return RString(averr_ssprintf(ret, "AVCodec: Couldn't open \"%s\"", file.c_str()));

	ret = avcodec::avformat_find_stream_info(av_format_context_, nullptr);
	if (ret < 0)
		return RString(averr_ssprintf(ret, "AVCodec (%s): Couldn't find codec parameters", file.c_str()));

	int stream_idx = avcodec::av_find_best_stream(av_format_context_, avcodec::AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
	if (stream_idx < 0 ||
		static_cast<unsigned int>(stream_idx) >= av_format_context_->nb_streams ||
		av_format_context_->streams[stream_idx] == nullptr)
		return "Couldn't find any video streams";
	av_stream_ = av_format_context_->streams[stream_idx];
	av_stream_codec_ = avcodec::avcodec_alloc_context3(nullptr);
	if (avcodec::avcodec_parameters_to_context(av_stream_codec_, av_stream_->codecpar) < 0)
		return ssprintf("Could not get context from parameters");

	if (av_stream_codec_->codec_id == avcodec::AV_CODEC_ID_NONE)
		return ssprintf("Unsupported codec %08x", av_stream_codec_->codec_tag);

	RString sError = OpenCodec();
	if (!sError.empty())
		return ssprintf("AVCodec (%s): %s", file.c_str(), sError.c_str());

	LOG->Trace("Bitrate: %i", static_cast<int>(av_stream_codec_->bit_rate));
	LOG->Trace("Codec pixel format: %s", avcodec::av_get_pix_fmt_name(av_stream_codec_->pix_fmt));
	total_frames_ = av_stream_->nb_frames;

	// Sometimes we might not get a correct frame count.
	// In that case, approximate and fix it later.
	if (total_frames_ <= 0) {
		total_frames_ = av_format_context_->duration // microseconds
			* (av_stream_->avg_frame_rate.num) / (av_stream_->avg_frame_rate.den) / (1000000);
		LOG->Trace("Number of frames provided is inaccurate, estimating.");
	}

	// This implies the video file might be missing some information, but it
	// might still be playable. Set total_frames_ to an arbitrary value and
	// the code will expand or shrink it later. Empty packets are cheap to
	// store, so it's fine if this value overshoots.
	if (total_frames_ <= 0) {
		LOG->Trace("Unable to estimate the total number of frames. Setting to 2000.");
		total_frames_ = 2000;
	}

	// Resize frame buffer based on video resolution and target memory budget.
	size_t optimal_buffer_size = CalculateFrameBufferSize(av_stream_codec_->width, av_stream_codec_->height);
	if (optimal_buffer_size < frame_buffer_.size()) {
		frame_buffer_.resize(optimal_buffer_size);
	} else if (optimal_buffer_size > frame_buffer_.size()) {
		while (frame_buffer_.size() < optimal_buffer_size) {
			frame_buffer_.emplace_back(std::make_unique<FrameHolder>());
		}
	}
	// Additional safeguard: if video is shorter than buffer, shrink to match.
	if (total_frames_ < frame_buffer_.size()) {
		LOG->Trace("Video shorter than frame buffer (%zu frames vs %zu slots), shrinking the buffer.",
			total_frames_, frame_buffer_.size());
		frame_buffer_.resize(total_frames_);
	}
	LOG->Trace("Frame buffer finalized: %zu slots for %dx%d video at %.1f MB per frame, ~%.1f MB total.",
		frame_buffer_.size(), av_stream_codec_->width, av_stream_codec_->height,
		(av_stream_codec_->width * av_stream_codec_->height * kBytesPerPixelBGRA) / (1024.0 * 1024.0),
		(frame_buffer_.size() * av_stream_codec_->width * av_stream_codec_->height * kBytesPerPixelBGRA) / (1024.0 * 1024.0));
	LOG->Trace("Number of frames detected: %zu", total_frames_);

	return RString();
}

RString MovieDecoder_FFMpeg::OpenCodec()
{
	Init();

	ASSERT(av_stream_ != nullptr);
	if (av_stream_codec_->codec)
		avcodec::avcodec_close(av_stream_codec_);

	const avcodec::AVCodec* pCodec = avcodec::avcodec_find_decoder(av_stream_codec_->codec_id);
	if (pCodec == nullptr)
		return ssprintf("Couldn't find decoder %i", av_stream_codec_->codec_id);

	av_stream_codec_->workaround_bugs = 1;
	av_stream_codec_->idct_algo = FF_IDCT_AUTO;
	av_stream_codec_->error_concealment = 3;

	LOG->Trace("Opening codec %s", pCodec->name);

	int ret = avcodec::avcodec_open2(av_stream_codec_, pCodec, nullptr);
	if (ret < 0)
		return RString(averr_ssprintf(ret, "Couldn't open codec \"%s\"", pCodec->name));
	ASSERT(av_stream_codec_->codec != nullptr);

	return RString();
}

void MovieDecoder_FFMpeg::Close()
{
	if (av_stream_ && av_stream_codec_->codec)
	{
		avcodec::avcodec_close(av_stream_codec_);
		av_stream_ = nullptr;
	}

	if (av_format_context_)
	{
		avcodec::avformat_close_input(&av_format_context_);
		av_format_context_ = nullptr;
	}
}

void MovieDecoder_FFMpeg::Rewind()
{
	display_frame_num_ = 0;
	reset_ = true;
}

void MovieDecoder_FFMpeg::Rollover()
{
	display_frame_num_ = 0;
	offset_ = next_offset_;
	next_offset_ = 0;
}

size_t MovieDecoder_FFMpeg::CalculateFrameBufferSize(int width, int height)
{
	if (width <= 0 || height <= 0) {
		return kFrameBufferMinSlots;
	}

	size_t frame_size_bytes = static_cast<size_t>(width) * static_cast<size_t>(height) * kBytesPerPixelBGRA;

	size_t optimal_slots = kFrameBufferTargetMemory / frame_size_bytes;

	return std::max(optimal_slots, kFrameBufferMinSlots);
}

size_t MovieDecoder_FFMpeg::GetFrameBufferIndex(size_t logical_frame_num) const
{
	// Apply the offset (used for looping) and wrap around the buffer size
	ASSERT(frame_buffer_.size() > 0);
	return (logical_frame_num + offset_) % frame_buffer_.size();
}

RageSurface* MovieDecoder_FFMpeg::CreateCompatibleSurface(int iTextureWidth, int iTextureHeight, bool bPreferHighColor, MovieDecoderPixelFormatYCbCr& fmtout)
{
	return RageMovieTextureDriver_FFMpeg::AVCodecCreateCompatibleSurface(iTextureWidth, iTextureHeight, bPreferHighColor, *ConvertValue<int>(&av_pixel_format_), fmtout);
}

MovieTexture_FFMpeg::MovieTexture_FFMpeg(RageTextureID ID) :
	MovieTexture_Generic(ID, std::make_unique<MovieDecoder_FFMpeg>())
{
}

RageMovieTexture* RageMovieTextureDriver_FFMpeg::Create(RageTextureID ID, RString& sError)
{
	MovieTexture_FFMpeg* pRet = new MovieTexture_FFMpeg(ID);
	sError = pRet->Init();
	if (!sError.empty())
		RageUtil::SafeDelete(pRet);
	return pRet;
}

REGISTER_MOVIE_TEXTURE_CLASS(FFMpeg);

/*
 * (c) 2003-2005 Glenn Maynard
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
