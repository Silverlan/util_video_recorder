/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __UTIL_VIDEO_RECORDER_HPP__
#define __UTIL_VIDEO_RECORDER_HPP__

#include <cinttypes>
#include <string>
#include <vector>
#include <array>
#include <memory>
#include <optional>
#include <functional>
#include <chrono>
#include <util_image_buffer.hpp>

// Note: H264 uses the GPL license
#define VIDEO_RECORDER_ENABLE_H264_CODEC

class FFMpegEncoder;
class VideoRecorder
{
public:
	enum class Codec : uint32_t
	{
		Raw,
		MPEG4,
#ifdef VIDEO_RECORDER_ENABLE_H264_CODEC
		H264,
#endif
		OpenH264,
		VP8,
		VP9,
		Dirac,
		AV1,
		MotionJPEG,
		MPEG1,
		HEVC,

		Count
	};
	enum class Format : uint32_t
	{
		Raw,
		WebM,
		Matroska,
		Flash,
		F4V,
		SWF,
		Vob,
		Ogg,
		Dirac,
		GIF,
		AVI,
		QuickTime,
		RealMedia,
		MPEG4,
		MPEG1,
		MPEG2,
		M4V,
		ThreeGPP,
		ThreeGPP2,

		Count
	};
	enum class Quality : uint32_t
	{
		VeryLow = 0,
		Low,
		Medium,
		High,
		VeryHigh
	};
	using LogicError = std::logic_error;
	using RuntimeError = std::runtime_error;

	using ThreadIndex = uint32_t;
	using FrameRate = uint32_t;
	using BitRate = uint32_t;
	using ColorComponent = uint8_t;
	using Color = std::array<ColorComponent,4>;
	using FrameData = std::vector<Color>;
	struct EncodingSettings
	{
		uint32_t width = 1'024;
		uint32_t height = 768;
		Codec codec = Codec::MPEG4;
		Format format = Format::AVI;
		FrameRate frameRate = 60;
		std::optional<BitRate> bitRate = {};
		Quality quality = Quality::VeryHigh;
	};
	struct ICustomFile
	{
		virtual ~ICustomFile()=default;
		virtual bool open(const std::string &fileName)=0;
		virtual void close()=0;
		virtual std::optional<uint64_t> write(const uint8_t *data, size_t size)=0;
		virtual std::optional<uint64_t> read(uint8_t *data, size_t size)=0;
		virtual std::optional<uint64_t> seek(int64_t offset, int whence)=0;
	};
	static std::unique_ptr<VideoRecorder> Create(std::unique_ptr<ICustomFile> fileInterface);
	static std::vector<Codec> GetSupportedCodecs(Format format);
	static std::vector<Format> GetAllFormats();
	static std::vector<Codec> GetAllCodecs();
	static std::string FormatToName(Format format);
	static std::string CodecToName(Codec codec);
	static BitRate CalcBitrate(uint32_t width,uint32_t height,FrameRate frameRate,double bitsPerPixel);
	static double GetBitsPerPixel(Quality quality);
	~VideoRecorder();

	void StartRecording(const std::string &outFileName,const EncodingSettings &encodingSettings);
	void EndRecording();
	bool IsRecording() const;
	ThreadIndex StartFrame();
	int32_t WriteFrame(const uimg::ImageBuffer &imgBuf,double frameTime);

	uint32_t GetWidth() const;
	uint32_t GetHeight() const;
	std::chrono::nanoseconds GetEncodingDuration() const;
	std::pair<uint32_t,uint32_t> GetResolution() const;
private:
	VideoRecorder(std::unique_ptr<ICustomFile> fileInterface);
	std::shared_ptr<FFMpegEncoder> m_ffmpegEncoder = nullptr;
	std::shared_ptr<ICustomFile> m_fileInterface = nullptr;
};

#endif
