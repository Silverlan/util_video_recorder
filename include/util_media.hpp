/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __UTIL_MEDIA_HPP__
#define __UTIL_MEDIA_HPP__

// Note: H264 uses the GPL license
#define VIDEO_RECORDER_ENABLE_H264_CODEC

#include <memory>
#include <string>
#include <optional>
#include <vector>
#include <stdexcept>

namespace media
{
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

	struct ICustomFile
	{
		virtual ~ICustomFile()=default;
		virtual bool open(const std::string &fileName)=0;
		virtual void close()=0;
		virtual std::optional<uint64_t> write(const uint8_t *data, size_t size)=0;
		virtual std::optional<uint64_t> read(uint8_t *data, size_t size)=0;
		virtual std::optional<uint64_t> seek(int64_t offset, int whence)=0;
	};

	using FrameRate = uint32_t;
	using BitRate = uint32_t;
	using ColorComponent = uint8_t;
	using Color = std::array<ColorComponent,4>;
	using FrameData = std::vector<Color>;

	std::vector<Codec> get_supported_codecs(Format format);
	std::vector<Format> get_all_formats();
	std::vector<Codec> get_all_codecs();
	std::string format_to_name(Format format);
	std::string codec_to_name(Codec codec);
	BitRate calc_bitrate(uint32_t width,uint32_t height,FrameRate frameRate,double bitsPerPixel);
	double get_bits_per_pixel(Quality quality);

	using LogicError = std::logic_error;
	using RuntimeError = std::runtime_error;
};

#endif
