/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __FFMPEG_WORKER_THREADS_HPP__
#define __FFMPEG_WORKER_THREADS_HPP__

#include <queue>
#include <atomic>
#include <thread>
#include <mutex>
#include "ffmpeg_encoder.hpp"

class BaseVideoThread
{
public:
	bool IsValid() const {return m_hasError == false;}
	std::optional<std::error_code> GetErrorCode() const {return m_hasError ? m_errorCode : std::optional<std::error_code>{};}
protected:
	bool CheckError(std::error_code err)
	{
		if(m_hasError || !err)
			return !!err;
		m_errorCode = err;
		m_hasError = true;
		return !!err;
	}
private:
	std::error_code m_errorCode;
	std::atomic<bool> m_hasError = false;
};

class VideoPacketWriterThread
	: public BaseVideoThread
{
public:
	VideoPacketWriterThread(av::FormatContext &formatContext);
	~VideoPacketWriterThread();
	void Start();
	void Stop(std::optional<FFMpegEncoder::FrameIndex> waitUntilFrameIndex={});
	void AddPacket(const av::Packet &packet,FFMpegEncoder::FrameIndex frameIndex);
private:
	void WritePacket(const av::Packet &packet);
	void Run();

	std::thread m_thread;
	std::atomic<bool> m_running = false;
	std::atomic<FFMpegEncoder::FrameIndex> m_nextPacketFrameIndex = 0;
	std::condition_variable m_waitForFinalFrame = {};
	av::FormatContext &m_formatContext;
	std::vector<std::pair<FFMpegEncoder::FrameIndex,av::Packet>> m_packetQueue = {};
	std::mutex m_packetQueueMutex = {};
};

class VideoEncoderThread
	: public BaseVideoThread
{
public:
	VideoEncoderThread(
		VideoPacketWriterThread &writerThread,av::VideoEncoderContext &encoder,const VideoRecorder::EncodingSettings &encodingSettings,
		av::PixelFormat dstPixelFormat
	);
	~VideoEncoderThread();
	bool IsBusy() const;
	void EncodeFrame(FFMpegEncoder::FrameIndex frameIndex,const uimg::ImageBuffer &imgBuf);
	std::chrono::steady_clock::duration GetWorkDuration() const;
	void Start();
	void Stop();
private:
	static void InitFrameFromBufferData(av::VideoFrame &frame,const uimg::ImageBuffer &imgBuf);
	void Run();
	void EncodeCurrentFrame();

	void *m_dataBuffer = nullptr;
	FFMpegEncoder::FrameIndex m_frameIndex = 0;
	std::shared_ptr<const uimg::ImageBuffer> m_currentFrameImageBuffer = nullptr;
	std::atomic<bool> m_isEncodingFrame = false;
	std::thread m_thread;
	std::atomic<bool> m_running = false;
	av::VideoRescaler m_videoRescaler = {};
	av::VideoFrame m_srcFrame;
	av::VideoFrame m_dstFrame;
	av::VideoEncoderContext &m_encoder;
	std::chrono::steady_clock::time_point m_startTime;

	VideoPacketWriterThread &m_writerThread;
};

#endif
