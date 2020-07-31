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
#include "util_media.hpp"

namespace media
{
	class FFMpegEncoder;
	class VideoRecorder
	{
	public:
		using ThreadIndex = uint32_t;
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
		static std::unique_ptr<VideoRecorder> Create(std::unique_ptr<ICustomFile> fileInterface);
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
};

#endif
