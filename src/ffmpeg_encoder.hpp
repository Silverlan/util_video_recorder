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

class VideoPacketWriterThread;
class VideoEncoderThread;
class FFMpegEncoder
{
public:
	static const auto SOURCE_PIXEL_FORMAT = AV_PIX_FMT_RGB24;
	static const auto FRAME_ALIGNMENT = 32u;

	struct AVFileIO
		: public av::CustomIO
	{
		AVFileIO(const std::shared_ptr<VideoRecorder::ICustomFile> &fileInterface);
		bool open(const std::string &fileName);
		virtual ssize_t write(const uint8_t *data, size_t size) override;
		virtual ssize_t read(uint8_t *data, size_t size) override;
		virtual int64_t seek(int64_t offset, int whence) override;
		virtual int seekable() const override;
		virtual const char *name() const override;
	private:
		std::string m_fileName;
		std::shared_ptr<VideoRecorder::ICustomFile> m_fileInterface = nullptr;
	};
	
	using FrameIndex = uint32_t;
	static std::unique_ptr<FFMpegEncoder> Create(
		const std::string &outFileName,const VideoRecorder::EncodingSettings &encodingSettings,const std::shared_ptr<VideoRecorder::ICustomFile> &fileInterface=nullptr
	);

	VideoRecorder::ThreadIndex StartFrame();
	int32_t WriteFrame(const uimg::ImageBuffer &imgBuf,double frameTime);
	void EndRecording();
	uint32_t GetWidth() const;
	uint32_t GetHeight() const;
	std::chrono::nanoseconds GetEncodingDuration() const;
private:
	FFMpegEncoder()=default;
	void CheckError(std::error_code errCode);
	void Initialize(const std::string &outFileName,const VideoRecorder::EncodingSettings &encodingSettings,const std::shared_ptr<VideoRecorder::ICustomFile> &fileInterface=nullptr);
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

#endif
