/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "util_video_recorder.hpp"
#include "ffmpeg_encoder.hpp"

std::unique_ptr<VideoRecorder> VideoRecorder::Create(std::unique_ptr<ICustomFile> fileInterface)
{
	return std::unique_ptr<VideoRecorder>{new VideoRecorder{std::move(fileInterface)}};
}

static std::array<std::string,static_cast<std::underlying_type_t<VideoRecorder::Format>>(VideoRecorder::Format::Count)> s_formatToString = {
	"rawvideo",
	"webm",
	"matroska",
	"flv",
	"f4v",
	"swf",
	"vob",
	"ogg",
	"dirac",
	"gif",
	"avi",
	"mov",
	"rm",
	"mp4",
	"mpeg",
	"mpeg2video",
	"m4v",
	"3gp",
	"3g2"
};
std::string VideoRecorder::FormatToName(Format format) {return s_formatToString.at(static_cast<std::underlying_type_t<decltype(format)>>(format));}

static std::array<std::string,static_cast<std::underlying_type_t<VideoRecorder::Codec>>(VideoRecorder::Codec::Count)> s_codecToString = {
	"rawvideo",
	"mpeg4",
#ifdef VIDEO_RECORDER_ENABLE_H264_CODEC
	"libx264",
#endif
	"libopenh264",
	"vp8",
	"vp9",
	"dirac",
	"av1",
	"mjpeg",
	"mpeg1video",
	"hevc"
};
std::string VideoRecorder::CodecToName(Codec codec) {return s_codecToString.at(static_cast<std::underlying_type_t<decltype(codec)>>(codec));}
VideoRecorder::BitRate VideoRecorder::CalcBitrate(uint32_t width,uint32_t height,FrameRate frameRate,double bitsPerPixel)
{
	return width *height *frameRate *bitsPerPixel;
}
double VideoRecorder::GetBitsPerPixel(Quality quality)
{
	const auto lowestBpp = 0.05;
	const auto highestBpp = 0.15;
	switch(quality)
	{
		case Quality::VeryLow:
			return lowestBpp;
		case Quality::Low:
			return lowestBpp +(highestBpp -lowestBpp) *0.25;
		case Quality::Medium:
			return lowestBpp +(highestBpp -lowestBpp) *0.5;
		case Quality::High:
			return lowestBpp +(highestBpp -lowestBpp) *0.75;
		case Quality::VeryHigh:
		default:
			return highestBpp;
	}
}
std::vector<VideoRecorder::Format> VideoRecorder::GetAllFormats()
{
	auto numFormats = static_cast<std::underlying_type_t<VideoRecorder::Format>>(VideoRecorder::Format::Count);
	std::vector<VideoRecorder::Format> formats {};
	formats.reserve(numFormats);
	for(auto i=decltype(numFormats){0};i<numFormats;++i)
		formats.push_back(static_cast<VideoRecorder::Format>(i));
	return formats;
}
std::vector<VideoRecorder::Codec> VideoRecorder::GetAllCodecs()
{
	auto numCodecs = static_cast<std::underlying_type_t<VideoRecorder::Codec>>(VideoRecorder::Codec::Count);
	std::vector<VideoRecorder::Codec> codecs {};
	codecs.reserve(numCodecs);
	for(auto i=decltype(numCodecs){0};i<numCodecs;++i)
		codecs.push_back(static_cast<VideoRecorder::Codec>(i));
	return codecs;
}
std::vector<VideoRecorder::Codec> VideoRecorder::GetSupportedCodecs(Format format)
{
	auto formatName = FormatToName(format);
	av::OutputFormat avFormat {};
	if(avFormat.setFormat(formatName) == false)
		return {};
	auto numCodecs = static_cast<std::underlying_type_t<Codec>>(Codec::Count);
	std::vector<VideoRecorder::Codec> supportedCodecs {};
	supportedCodecs.reserve(numCodecs);
	for(auto i=decltype(numCodecs){0u};i<numCodecs;++i)
	{
		auto codec = static_cast<Codec>(i);
		auto avCodec = av::findEncodingCodec(CodecToName(codec));
		if(avFormat.codecSupported(avCodec) == false)
			continue;
		supportedCodecs.push_back(codec);
	}
	return supportedCodecs;
}
VideoRecorder::VideoRecorder(std::unique_ptr<ICustomFile> fileInterface)
	: m_fileInterface{std::move(fileInterface)}
{}
VideoRecorder::~VideoRecorder()
{
	EndRecording();
}
VideoRecorder::ThreadIndex VideoRecorder::StartFrame()
{
	if(IsRecording() == false)
		return 0;
	return m_ffmpegEncoder->StartFrame();
}
int32_t VideoRecorder::WriteFrame(const Color *buffer,size_t size,double frameTime)
{
	if(IsRecording() == false)
		return -1;
	return m_ffmpegEncoder->WriteFrame(buffer,size,frameTime);
}
uint32_t VideoRecorder::GetWidth() const {return m_ffmpegEncoder->GetWidth();}
uint32_t VideoRecorder::GetHeight() const {return m_ffmpegEncoder->GetHeight();}
std::chrono::nanoseconds VideoRecorder::GetEncodingDuration() const {return m_ffmpegEncoder ? m_ffmpegEncoder->GetEncodingDuration() : std::chrono::nanoseconds{0};}
std::pair<uint32_t,uint32_t> VideoRecorder::GetResolution() const {return {GetWidth(),GetHeight()};}
bool VideoRecorder::IsRecording() const {return m_ffmpegEncoder != nullptr;}
void VideoRecorder::StartRecording(const std::string &outFileName,const EncodingSettings &encodingSettings)
{
	if(IsRecording())
		EndRecording(); // End previous recording session

	m_ffmpegEncoder = FFMpegEncoder::Create(outFileName,encodingSettings,m_fileInterface);
}
void VideoRecorder::EndRecording()
{
	if(IsRecording() == false)
		return;
	m_ffmpegEncoder->EndRecording();
	m_ffmpegEncoder = nullptr;
	m_fileInterface->close();
}
