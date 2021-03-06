/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ffmpeg_encoder.hpp"
#include "ffmpeg_worker_threads.hpp"
#include "util_ffmpeg.hpp"
#include <avutils.h>

using namespace media;

#pragma optimize("",off)
std::unique_ptr<FFMpegEncoder> FFMpegEncoder::Create(
	const std::string &outFileName,const VideoRecorder::EncodingSettings &encodingSettings,
	const std::shared_ptr<ICustomFile> &fileInterface
)
{
	auto encoder = std::unique_ptr<FFMpegEncoder>{new FFMpegEncoder{}};
	encoder->Initialize(outFileName,encodingSettings,fileInterface);
	return encoder;
}

void FFMpegEncoder::Initialize(const std::string &outFileName,const VideoRecorder::EncodingSettings &encodingSettings,const std::shared_ptr<ICustomFile> &fileInterface)
{
    av::init();
	
	av::set_logging_level(AV_LOG_DEBUG);

	auto strFormat = format_to_name(encodingSettings.format);
	av::OutputFormat outputFormat {};
	if(outputFormat.setFormat(strFormat,outFileName) == false)
		throw RuntimeError{"Output format '" +strFormat +"' could not be set!"};
	
	m_formatContext.setFormat(outputFormat);

	auto strCodec = codec_to_name(encodingSettings.codec);
	auto avCodec = av::findEncodingCodec(strCodec);
	if(outputFormat.codecSupported(avCodec) == false)
		throw LogicError{"Codec '" +strCodec +"' is not supported by output format '" +strFormat +"'!"};

	std::error_code errCode;
	m_outputStream = m_formatContext.addStream(avCodec,errCode);
	check_error(errCode);

	m_encoder = std::make_unique<av::VideoEncoderContext>(m_outputStream);
	auto &encoder = *m_encoder;

	switch(encodingSettings.quality)
	{
		case Quality::VeryLow:
			encoder.setGlobalQuality(FF_LAMBDA_MAX);
			break;
		case Quality::Low:
			encoder.setGlobalQuality(FF_LAMBDA_MAX *0.75);
			break;
		case Quality::Medium:
			encoder.setGlobalQuality(FF_LAMBDA_MAX *0.5);
			break;
		case Quality::High:
			encoder.setGlobalQuality(FF_LAMBDA_MAX *0.25);
			break;
		case Quality::VeryHigh:
			encoder.setGlobalQuality(0);
			break;
	}
	encoder.addFlags(AV_CODEC_FLAG_QSCALE);

	auto *pRawEncoder = encoder.raw();
	pRawEncoder->thread_count = 4;
    av::PixelFormat dstPixelFormat {AVPixelFormat::AV_PIX_FMT_YUV420P};
	switch(encodingSettings.codec)
	{
		case Codec::MotionJPEG:
			// Deprecated in favor of AV_PIX_FMT_YUV420P according to ffmpeg log, but
			// AV_PIX_FMT_YUV420P does not work with MotionJPEG
			dstPixelFormat = AVPixelFormat::AV_PIX_FMT_YUVJ420P;

			pRawEncoder->thread_count = 1;
			break;
	}
    encoder.setPixelFormat(dstPixelFormat);

    // Settings
    encoder.setWidth(encodingSettings.width);
    encoder.setHeight(encodingSettings.height);
    encoder.setTimeBase(av::Rational{1,static_cast<int32_t>(encodingSettings.frameRate)});
	uint64_t bitRate = 0;
	if(encodingSettings.bitRate.has_value())
		bitRate = *encodingSettings.bitRate;
	else
		bitRate = calc_bitrate(encodingSettings.width,encodingSettings.height,encodingSettings.frameRate,get_bits_per_pixel(encodingSettings.quality));
    encoder.setBitRate(bitRate);
    m_outputStream.setFrameRate({static_cast<int32_t>(encodingSettings.frameRate),1});
    m_outputStream.setTimeBase(encoder.timeBase());

	if(fileInterface == nullptr)
		m_formatContext.openOutput(outFileName,errCode);
	else
	{
		m_fileIo = std::unique_ptr<AVFileIO>{new AVFileIO{fileInterface}};
		if(m_fileIo->open(outFileName)	== true)
			m_formatContext.openOutput(m_fileIo.get(),errCode);
		else
			throw RuntimeError{"Unable to open file '" +outFileName +"'!"};
	}
	check_error(errCode);
    
    encoder.open(avCodec,errCode);
	check_error(errCode);

	m_formatContext.writeHeader(errCode);
	check_error(errCode);
	m_formatContext.flush();

	m_packetWriterThread = std::make_unique<VideoPacketWriterThread>(m_formatContext);
	m_packetWriterThread->Start();
	m_encoderThreads.resize(1); // MUST be 1, as some codecs do not support multi-threading this way!
	for(auto &thread : m_encoderThreads)
	{
		thread = std::make_shared<VideoEncoderThread>(*m_packetWriterThread,*m_encoder,encodingSettings,dstPixelFormat);
		thread->Start();
	}
}

VideoRecorder::ThreadIndex FFMpegEncoder::StartFrame()
{
	auto longestDuration = std::chrono::steady_clock::duration{std::chrono::nanoseconds{0}};
	auto bestCandidateIndex = std::numeric_limits<VideoRecorder::ThreadIndex>::max();
	// Find thread that is either not busy, or has been
	// busy the longest (in which case its likely to finish first)
	for(auto i=decltype(m_encoderThreads.size()){0u};i<m_encoderThreads.size();++i)
	{
		auto &pEncoderThread = m_encoderThreads.at(i);
		if(pEncoderThread->IsBusy() == false)
		{
			bestCandidateIndex = i;
			break;
		}
		auto dur = pEncoderThread->GetWorkDuration();
		if(dur < longestDuration)
			continue;
		longestDuration = dur;
		bestCandidateIndex = i;
	}
	if(bestCandidateIndex == std::numeric_limits<VideoRecorder::ThreadIndex>::max())
		throw LogicError{"No threads have been allocated for encoding!"};
	m_curThreadIndex = bestCandidateIndex;
	return m_curThreadIndex;
}

void FFMpegEncoder::EncodeFrame(const uimg::ImageBuffer &imgBuf)
{
	m_encoderThreads.at(m_curThreadIndex)->EncodeFrame(m_curFrameIndex,imgBuf);
}

void FFMpegEncoder::EndRecording()
{
	m_packetWriterThread->Stop(m_curFrameIndex);
	for(auto &thread : m_encoderThreads)
		thread->Stop();
	std::error_code errCode {};
	if(m_packetWriterThread->GetErrorCode().has_value())
		errCode = *m_packetWriterThread->GetErrorCode();
	check_error(errCode);
	m_packetWriterThread = nullptr;
	
	for(auto &pThread : m_encoderThreads)
	{
		if(pThread->GetErrorCode().has_value())
			errCode = *pThread->GetErrorCode();
		check_error(errCode);
	}
	m_encoderThreads.clear();

    m_formatContext.writePacket(errCode);
	check_error(errCode);
    m_formatContext.writeTrailer(errCode);
	check_error(errCode);
}

int32_t FFMpegEncoder::WriteFrame(const uimg::ImageBuffer &imgBuf,double frameTime)
{
	auto timeStamp = frameTime; // Timestamp to beginning of recording
	auto prevTimeStamp = m_prevTimeStamp; // TODO
	auto dtTimeStamp = timeStamp -prevTimeStamp;
	auto frameRate = static_cast<double>(m_outputStream.frameRate().getNumerator());
	dtTimeStamp *= frameRate;
	auto dtTimeUnits = floor(dtTimeStamp); // TODO: Framerate

	m_prevTimeStamp = timeStamp -(dtTimeStamp -dtTimeUnits) /frameRate;

	uint32_t deltaTime = dtTimeUnits;
	if(deltaTime == 0 && m_curFrameIndex > 0)
		return 0; // Skip this frame
	auto numFrames = std::max(deltaTime,1u);
	// Frame may have to be encoded multiple times
	auto tCur = std::chrono::steady_clock::now();
	for(auto i=decltype(numFrames){0u};i<numFrames;++i)
	{
		/* encode the image */
		EncodeFrame(imgBuf);
		++m_curFrameIndex;
	}
	auto tDelta = std::chrono::steady_clock::now() -tCur;
	m_encodeDuration += tDelta;
	return numFrames;
}
uint32_t FFMpegEncoder::GetWidth() const {return m_encoder->width();}
uint32_t FFMpegEncoder::GetHeight() const {return m_encoder->height();}
std::chrono::nanoseconds FFMpegEncoder::GetEncodingDuration() const {return m_encodeDuration;}
#pragma optimize("",on)
