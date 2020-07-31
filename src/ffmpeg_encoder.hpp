/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __FFMPEG_ENCODER_HPP__
#define __FFMPEG_ENCODER_HPP__

#include <av.h>
#include <frame.h>
#include <format.h>
#include <formatcontext.h>
#include <codeccontext.h>
#include <videorescaler.h>
#include <memory>
#include <string>
#include <vector>
#include <array>
#include <functional>
#include "util_video_recorder.hpp"
#include "util_ffmpeg.hpp"

namespace media
{
	class VideoPacketWriterThread;
	class VideoEncoderThread;
	struct AVFileIO;
	class FFMpegEncoder
	{
	public:
		static const auto SOURCE_PIXEL_FORMAT = AV_PIX_FMT_RGB24;
		static const auto FRAME_ALIGNMENT = 32u;
	
		using FrameIndex = uint32_t;
		static std::unique_ptr<FFMpegEncoder> Create(
			const std::string &outFileName,const VideoRecorder::EncodingSettings &encodingSettings,const std::shared_ptr<ICustomFile> &fileInterface=nullptr
		);

		VideoRecorder::ThreadIndex StartFrame();
		int32_t WriteFrame(const uimg::ImageBuffer &imgBuf,double frameTime);
		void EndRecording();
		uint32_t GetWidth() const;
		uint32_t GetHeight() const;
		std::chrono::nanoseconds GetEncodingDuration() const;
	private:
		FFMpegEncoder()=default;
		void Initialize(const std::string &outFileName,const VideoRecorder::EncodingSettings &encodingSettings,const std::shared_ptr<ICustomFile> &fileInterface=nullptr);
		void EncodeFrame(const uimg::ImageBuffer &imgBuf);

		std::unique_ptr<AVFileIO> m_fileIo = nullptr;
		av::FormatContext m_formatContext = {};
		av::Stream m_outputStream;
		std::unique_ptr<av::VideoEncoderContext> m_encoder;
		std::chrono::steady_clock::duration m_encodeDuration = std::chrono::seconds{0};
		FrameIndex m_curFrameIndex = 0;
		VideoRecorder::ThreadIndex m_curThreadIndex = 0;
		double m_prevTimeStamp = 0.0;

		std::shared_ptr<VideoPacketWriterThread> m_packetWriterThread = {};
		std::vector<std::shared_ptr<VideoEncoderThread>> m_encoderThreads = {};
	};
};

#endif
