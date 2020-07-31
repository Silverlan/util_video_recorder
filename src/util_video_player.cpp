/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "util_video_player.hpp"
#include "util_video_recorder.hpp"
#include "ffmpeg_decoder.hpp"

using namespace media;

#pragma optimize("",off)
std::unique_ptr<VideoPlayer> VideoPlayer::Create(VFilePtr f)
{
	auto ffmpegDecoder = FFMpegDecoder::Create(f);
	return ffmpegDecoder ? std::unique_ptr<VideoPlayer>{new VideoPlayer{ffmpegDecoder}} : nullptr;
}

std::shared_ptr<uimg::ImageBuffer> VideoPlayer::ReadFrame(double &outPts)
{
	return m_ffmpegDecoder->ReadFrame(outPts);
}

double VideoPlayer::GetVideoFrameRate() const {return m_ffmpegDecoder->GetVideoFrameRate();}
double VideoPlayer::GetAudioFrameRate() const {return m_ffmpegDecoder->GetAudioFrameRate();}
double VideoPlayer::GetAspectRatio() const {return m_ffmpegDecoder->GetAspectRatio();}
uint32_t VideoPlayer::GetWidth() const {return m_ffmpegDecoder->GetWidth();}
uint32_t VideoPlayer::GetHeight() const {return m_ffmpegDecoder->GetHeight();}

VideoPlayer::VideoPlayer(std::shared_ptr<FFMpegDecoder> ffmpegDecoder)
	: m_ffmpegDecoder{ffmpegDecoder}
{}
#pragma optimize("",on)
