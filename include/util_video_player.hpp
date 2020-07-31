/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __UTIL_VIDEO_PLAYER_HPP__
#define __UTIL_VIDEO_PLAYER_HPP__

#include <memory>
#include <string>
#include <fsys/filesystem.h>
#include "util_media.hpp"

namespace uimg {class ImageBuffer;};
namespace media
{
	class FFMpegDecoder;
	class VideoPlayer
	{
	public:
		static std::unique_ptr<VideoPlayer> Create(VFilePtr f);
		std::shared_ptr<uimg::ImageBuffer> ReadFrame(double &outPts);
		double GetVideoFrameRate() const;
		double GetAudioFrameRate() const;
		double GetAspectRatio() const;
		uint32_t GetWidth() const;
		uint32_t GetHeight() const;
	private:
		VideoPlayer(std::shared_ptr<FFMpegDecoder> ffmpegDecoder);

		std::shared_ptr<FFMpegDecoder> m_ffmpegDecoder = nullptr;
	};
};

#endif
