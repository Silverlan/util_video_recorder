/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ffmpeg_worker_threads.hpp"

#pragma optimize("",off)
#ifdef _WIN32
	#include <Windows.h>
#else
	#include <thread>
#endif

using namespace media;

enum class ThreadPriority : uint32_t
{
	Lowest = 0,
	Low,
	BelowNormal,
	Normal,
	AboveNormal,
	High,
	Highest
};
static void set_thread_priority(std::thread &thread,ThreadPriority priority)
{
	auto *threadHandle = thread.native_handle();
#ifdef _WIN32
	switch(priority)
	{
		case ThreadPriority::Lowest:
			SetThreadPriority(threadHandle,THREAD_PRIORITY_LOWEST);
			break;
		case ThreadPriority::Low:
		case ThreadPriority::BelowNormal:
			SetThreadPriority(threadHandle,THREAD_PRIORITY_BELOW_NORMAL);
			break;
		case ThreadPriority::Normal:
			SetThreadPriority(threadHandle,THREAD_PRIORITY_NORMAL);
			break;
		case ThreadPriority::AboveNormal:
			SetThreadPriority(threadHandle,THREAD_PRIORITY_ABOVE_NORMAL);
			break;
		case ThreadPriority::High:
		case ThreadPriority::Highest:
			SetThreadPriority(threadHandle,THREAD_PRIORITY_HIGHEST);
			break;
	}
#else
	auto minPriority = sched_get_priority_min(SCHED_OTHER);
	auto maxPriority = sched_get_priority_max(SCHED_OTHER);
	switch(priority)
	{
		case ThreadPriority::Lowest:
			pthread_setschedprio(threadHandle,minPriority);
			break;
		case ThreadPriority::Low:
			pthread_setschedprio(threadHandle,-10);
			break;
		case ThreadPriority::BelowNormal:
			pthread_setschedprio(threadHandle,-5);
			break;
		case ThreadPriority::Normal:
			pthread_setschedprio(threadHandle,0);
			break;
		case ThreadPriority::AboveNormal:
			pthread_setschedprio(threadHandle,5);
			break;
		case ThreadPriority::High:
			pthread_setschedprio(threadHandle,10);
			break;
		case ThreadPriority::Highest:
			pthread_setschedprio(threadHandle,maxPriority);
			break;
	}
#endif
}
VideoPacketWriterThread::VideoPacketWriterThread(av::FormatContext &formatContext)
	: m_formatContext{formatContext}
{}
VideoPacketWriterThread::~VideoPacketWriterThread()
{
	Stop();
}
void VideoPacketWriterThread::Start()
{
	m_running = true;
	m_thread = std::thread{[this]() {
		while(m_running && IsValid())
			Run();
	}};
	set_thread_priority(m_thread,ThreadPriority::AboveNormal);
}
void VideoPacketWriterThread::Stop(std::optional<FFMpegEncoder::FrameIndex> waitUntilFrameIndex)
{
	while(waitUntilFrameIndex.has_value() && m_nextPacketFrameIndex < waitUntilFrameIndex && IsValid())
		; // Wait until final frame has been written
	m_running = false;
	if(m_thread.joinable())
		m_thread.join();
}
void VideoPacketWriterThread::AddPacket(const av::Packet &packet,FFMpegEncoder::FrameIndex frameIndex)
{
	std::scoped_lock<std::mutex> lock{m_packetQueueMutex};

	auto it = std::find_if(m_packetQueue.begin(),m_packetQueue.end(),[frameIndex](const std::pair<FFMpegEncoder::FrameIndex,av::Packet> &pair) {
		return frameIndex < pair.first;
	});
	m_packetQueue.insert(it,std::pair<FFMpegEncoder::FrameIndex,av::Packet>{frameIndex,packet});
}
void VideoPacketWriterThread::WritePacket(const av::Packet &packet)
{
	std::error_code errCode {};
	m_formatContext.writePacket(packet,errCode);
	CheckError(errCode);
}
void VideoPacketWriterThread::Run()
{
	std::unique_lock<std::mutex> lock {m_packetQueueMutex};
	if(m_packetQueue.empty() || m_packetQueue.front().first != m_nextPacketFrameIndex)
		return; // Wait for correct packet
	auto packet = m_packetQueue.front().second;
	m_packetQueue.erase(m_packetQueue.begin());

	lock.unlock();

	WritePacket(packet);
		
	++m_nextPacketFrameIndex;
}

//////////////////

VideoEncoderThread::VideoEncoderThread(
	VideoPacketWriterThread &writerThread,av::VideoEncoderContext &encoder,const VideoRecorder::EncodingSettings &encodingSettings,
	av::PixelFormat dstPixelFormat
)
	: m_writerThread{writerThread},m_encoder{encoder}
{
	av::PixelFormat srcPixelFormat {AVPixelFormat::AV_PIX_FMT_RGBA};
	m_srcFrame = {srcPixelFormat,static_cast<int32_t>(encodingSettings.width),static_cast<int32_t>(encodingSettings.height),static_cast<int32_t>(FFMpegEncoder::FRAME_ALIGNMENT)};
	m_dstFrame = {dstPixelFormat,static_cast<int32_t>(encodingSettings.width),static_cast<int32_t>(encodingSettings.height),static_cast<int32_t>(FFMpegEncoder::FRAME_ALIGNMENT)};
	m_dstFrame.setTimeBase(encoder.timeBase());
	m_dstFrame.setStreamIndex(0);
	m_dstFrame.setPictureType();
}
VideoEncoderThread::~VideoEncoderThread()
{
	Stop();
}
void VideoEncoderThread::InitFrameFromBufferData(av::VideoFrame &frame,const uimg::ImageBuffer &imgBuf)
{
	/*
	av::VideoFrame frame {
		reinterpret_cast<const uint8_t*>(colors.data()),colors.size() *sizeof(colors.front()),
		format,static_cast<int32_t>(width),static_cast<int32_t>(height),static_cast<int32_t>(alignment)
	};*/
	// VideoFrame constructor has a bug as of 19-06-10, where only the first row of the source data would be used.
	// We will have to copy the data manually as a work-around

	std::array<uint8_t*,4> buf;
	std::array<int,4> lineSize;
	auto *bufferData = reinterpret_cast<const uint8_t*>(imgBuf.GetData());
	av_image_fill_arrays(buf.data(),lineSize.data(),bufferData,frame.pixelFormat(),frame.width(),frame.height(),FFMpegEncoder::FRAME_ALIGNMENT);
	if(lineSize.at(0) != frame.width() *sizeof(Color))
		throw LogicError{"Frame line size does not match input data line size!"};

	// No need to copy data
	//memcpy(frame.raw()->data[0],colors.data(),colors.size() *sizeof(colors.front()));
	frame.raw()->data[0] = static_cast<uint8_t*>(const_cast<uint8_t*>(bufferData));
	frame.setComplete(true);

	frame.raw()->pts = 0;
	frame.raw()->pkt_dts = 0;
}
bool VideoEncoderThread::IsBusy() const {return m_isEncodingFrame;}
void VideoEncoderThread::EncodeFrame(FFMpegEncoder::FrameIndex frameIndex,const uimg::ImageBuffer &imgBuf)
{
	while(IsBusy() && IsValid())
		;
	if(IsValid() == false)
		return;
	m_startTime = std::chrono::steady_clock::now();
	m_frameIndex = frameIndex;
	auto ptrBuf = imgBuf.shared_from_this();
	if(ptrBuf->GetFormat() != uimg::ImageBuffer::Format::RGBA8)
		ptrBuf = ptrBuf->Copy(uimg::ImageBuffer::Format::RGBA8);
	m_currentFrameImageBuffer = ptrBuf;
	InitFrameFromBufferData(m_srcFrame,imgBuf);

	auto calcSize = av_image_get_buffer_size(m_srcFrame.pixelFormat(),m_srcFrame.width(),m_srcFrame.height(),FFMpegEncoder::FRAME_ALIGNMENT);
	if(calcSize != imgBuf.GetSize())
		throw LogicError{"Data size does not match expected size for the specified format and resolution!"};

	m_isEncodingFrame = true;
}
std::chrono::steady_clock::duration VideoEncoderThread::GetWorkDuration() const {return std::chrono::steady_clock::now() -m_startTime;}
void VideoEncoderThread::Start()
{
	m_running = true;
	m_thread = std::thread{[this]() {
		while(m_running && IsValid())
			Run();
	}};
	set_thread_priority(m_thread,ThreadPriority::AboveNormal);
}
void VideoEncoderThread::Stop()
{
	while(IsBusy() && IsValid())
		;
	m_running = false;
	if(m_thread.joinable())
		m_thread.join();
}
void VideoEncoderThread::Run()
{
	if(IsBusy() == false)
		return;
	EncodeCurrentFrame();
}
void VideoEncoderThread::EncodeCurrentFrame()
{
	std::error_code errCode;
	m_videoRescaler.rescale(m_dstFrame,m_srcFrame,errCode);
	if(CheckError(errCode))
		return;
	m_dstFrame.raw()->pts = m_frameIndex;
	auto packet = m_encoder.encode(m_dstFrame,errCode);
	if(CheckError(errCode))
		return;
	packet.setPts(av::Timestamp{m_frameIndex,m_encoder.timeBase()});
	packet.setDts(av::Timestamp{m_frameIndex,m_encoder.timeBase()});
	packet.setDuration(1);
	packet.setStreamIndex(0);

	m_writerThread.AddPacket(packet,m_frameIndex);
	m_isEncodingFrame = false;
	m_currentFrameImageBuffer = nullptr;
}
#pragma optimize("",on)
