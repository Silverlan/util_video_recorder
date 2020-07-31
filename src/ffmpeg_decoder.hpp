/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __FFMPEG_DECODER_HPP__
#define __FFMPEG_DECODER_HPP__

#include <memory>
#include <array>
#include <av.h>
#include <frame.h>
#include <format.h>
#include <formatcontext.h>
#include <codeccontext.h>
#include <fsys/filesystem.h>
#include "util_media.hpp"

struct SwsContext;
namespace uimg {class ImageBuffer;};
namespace media
{
	struct AVFileIOFSys;
	class FFMpegDecoder
	{
	public:
		static std::shared_ptr<FFMpegDecoder> Create(VFilePtr f);
		std::shared_ptr<uimg::ImageBuffer> ReadFrame(double &outPts);
		double GetVideoFrameRate() const;
		double GetAudioFrameRate() const;
		double GetAspectRatio() const;
		uint32_t GetWidth() const;
		uint32_t GetHeight() const;
	private:
		FFMpegDecoder();

		std::unique_ptr<AVFileIOFSys> m_fileIo = nullptr;
		av::FormatContext m_formatContext = {};
		av::Stream m_videoInputStream;
		av::Stream m_audioInputStream;

		uint32_t m_width = 0;
		uint32_t m_height = 0;
		AVCodecParserContext *m_parser = nullptr;
		std::shared_ptr<uimg::ImageBuffer> m_frame = nullptr;
		SwsContext *m_swsContext = nullptr;
		av::Packet m_packet {};

		std::unique_ptr<av::VideoDecoderContext> m_videoCodecContext = nullptr;
		std::unique_ptr<av::AudioDecoderContext> m_audioCodecContest = nullptr;
		std::array<uint8_t,4'096 +AV_INPUT_BUFFER_PADDING_SIZE> m_buffer {};
	};
};

#endif
