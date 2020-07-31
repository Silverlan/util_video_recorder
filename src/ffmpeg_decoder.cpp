/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ffmpeg_decoder.hpp"
#include "util_ffmpeg.hpp"
#include <util_image_buffer.hpp>
extern "C" {
	#include <libavutil/opt.h>
	#include <libavcodec/avcodec.h>
	#include <libavformat/avformat.h>
	#include <libswresample/swresample.h>
}

using namespace media;
#pragma optimize("",off)
FFMpegDecoder::FFMpegDecoder()
{}
std::shared_ptr<FFMpegDecoder> FFMpegDecoder::Create(VFilePtr f)
{
	av::init();
	//av::set_logging_level(AV_LOG_DEBUG);

	auto decoder = std::shared_ptr<FFMpegDecoder>{new FFMpegDecoder{}};

	/* set end of buffer to 0 (this ensures that no overreading happens for damaged MPEG streams) */
	std::fill(decoder->m_buffer.begin() +(decoder->m_buffer.size() -AV_INPUT_BUFFER_PADDING_SIZE),decoder->m_buffer.end(),0);

	std::error_code errCode;
	decoder->m_fileIo = std::make_unique<AVFileIOFSys>(f);
	decoder->m_formatContext.openInput(decoder->m_fileIo.get(),av::InputFormat{},errCode);
	check_error(errCode);

	decoder->m_formatContext.findStreamInfo(errCode);
	check_error(errCode);

	std::optional<av::Stream> videoStream {};
	av::Codec videoCodec;
	std::optional<av::Stream> audioStream {};
	av::Codec audioCodec;
	auto numStreams = decoder->m_formatContext.streamsCount();
	for(auto i=decltype(numStreams){0};i<numStreams;++i)
	{
		auto stream = decoder->m_formatContext.stream(i);
		if(stream.isVideo())
		{
			videoStream = stream;
			videoCodec = av::findDecodingCodec(decoder->m_formatContext.raw()->streams[i]->codec->codec_id);
		}
		else if(stream.isAudio())
		{
			audioStream = stream;
			audioCodec = av::findDecodingCodec(decoder->m_formatContext.raw()->streams[i]->codec->codec_id);
		}
	}

	if(videoStream)
	{
		decoder->m_videoInputStream = *videoStream;
		decoder->m_videoCodecContext = std::make_unique<av::VideoDecoderContext>(decoder->m_videoInputStream);
		decoder->m_videoCodecContext->open(videoCodec,errCode);
		check_error(errCode);
	}

	if(audioStream)
	{
		decoder->m_audioInputStream = *audioStream;
		decoder->m_audioCodecContest = std::make_unique<av::AudioDecoderContext>(decoder->m_audioInputStream);
		decoder->m_audioCodecContest->open(audioCodec,errCode);
		check_error(errCode);
	}
	return decoder;
}
static int64_t FrameToPts(AVStream* pavStream, int frame)
{
	return (int64_t(frame) * pavStream->r_frame_rate.den *  pavStream->time_base.den) / 
		(int64_t(pavStream->r_frame_rate.num) * 
			pavStream->time_base.num);
}
bool seekFrame(AVFormatContext *_format_ctx,AVCodecContext *_codec_ctx,AVStream* _video_stream,int FrameIndex)
{
	// Seek is done on packet dts
	int64_t target_dts_usecs = (int64_t)round(FrameIndex * (double)_video_stream->r_frame_rate.den / _video_stream->r_frame_rate.num * AV_TIME_BASE);
	// Remove first dts: when non zero seek should be more accurate
	auto first_dts_usecs = (int64_t)round(_video_stream->first_dts * (double)_video_stream->time_base.num / _video_stream->time_base.den * AV_TIME_BASE);
	target_dts_usecs += first_dts_usecs;
	int rv = av_seek_frame(_format_ctx, -1, target_dts_usecs, AVSEEK_FLAG_BACKWARD);
	if (rv < 0)
		return false;
		;//throw exception();

	avcodec_flush_buffers(_codec_ctx);
	return true;
}
#if 0
void AV_seek(AV * av, size_t frame)
{
	int frame_delta = frame - av->frame_id;
	if (frame_delta < 0 || frame_delta > 5)
		av_seek_frame(av->fmt_ctx, av->video_stream_id,
			frame, AVSEEK_FLAG_BACKWARD);
	while (av->frame_id != frame)
		AV_read_frame(av);
}
#endif
#if 0
static void decode_test(AVCodecContext *dec_ctx, AVFrame *frame, AVPacket *pkt,
	const char *filename)
{
	char buf[1024];
	int ret;

	ret = avcodec_send_packet(dec_ctx, pkt);
	if (ret < 0) {
		fprintf(stderr, "Error sending a packet for decoding\n");
		exit(1);
	}

	while (ret >= 0) {
		ret = avcodec_receive_frame(dec_ctx, frame);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
			return;
		else if (ret < 0) {
			fprintf(stderr, "Error during decoding\n");
			exit(1);
		}

		printf("saving frame %3d\n", dec_ctx->frame_number);
		fflush(stdout);

		/* the picture is allocated by the decoder. no need to
		free it */
		snprintf(buf, sizeof(buf), "%s-%d", filename, dec_ctx->frame_number);
		//pgm_save(frame->data[0], frame->linesize[0],
		//	frame->width, frame->height, buf);
	}
}
#endif

double FFMpegDecoder::GetVideoFrameRate() const {return m_videoInputStream.frameRate().getDouble();}
double FFMpegDecoder::GetAudioFrameRate() const {return m_audioInputStream.frameRate().getDouble();}
double FFMpegDecoder::GetAspectRatio() const {return (m_width > 0 && m_height > 0) ? (m_width /static_cast<double>(m_height)) : 1.0;}
uint32_t FFMpegDecoder::GetWidth() const {return m_width;}
uint32_t FFMpegDecoder::GetHeight() const {return m_height;}

std::shared_ptr<uimg::ImageBuffer> FFMpegDecoder::ReadFrame(double &outPts)
{
	//m_formatContext.seek({20,{1,1}});
	/*if(stream_index>=0){
		seek_target= av_rescale_q(seek_target, AV_TIME_BASE_Q,
			pFormatCtx->streams[stream_index]->time_base);
	}
	if(av_seek_frame(is->pFormatCtx, stream_index, 
		seek_target, is->seek_flags) < 0) {
		fprintf(stderr, "%s: error while seeking\n",
			is->pFormatCtx->filename);
	} else {*/
	{
		//this->m_videoCodecContext->raw(
		//auto frameIndex = 1000000;
		//seekFrame(m_formatContext.raw(),m_videoCodecContext->raw(),m_videoInputStream.raw(),frameIndex);
		//auto iSeekTarget = FrameToPts(m_videoInputStream.raw(),frameIndex);
		//auto iSuccess = av_seek_frame(m_formatContext.raw(), m_videoInputStream.index(), 
		//	iSeekTarget, AVSEEK_FLAG_FRAME);

		//AVPacket avPacket;
		//iRet = av_read_frame(m_pAVFmtCtx, &avPacket);
	}
	//av_seek_frame(m_formatContext.raw(),m_videoInputStream.index(),100,AVSEEK_FLAG_FRAME);
	//m_formatContext.seek({20,{1,1}});

	// ???????
	//int64_t seek_to = 100;
	//av_seek_frame(m_formatContext.raw(),m_videoInputStream.raw()->index,seek_to *m_videoInputStream.raw()->time_base.den / m_videoInputStream.raw()->time_base.num ,AVSEEK_FLAG_ANY);

	//avcodec_flush_buffers(m_videoCodecContext->raw());

	auto *frameT = av_frame_alloc();
	auto *pkt = av_packet_alloc();
	//decode_test(m_videoCodecContext->raw(),frameT,pkt,"");


#if 0
	std::error_code ec;
	while (av::Packet pkt = m_formatContext.readPacket(ec)) {
		if (ec) {
			std::cout << "Packet reading error: " << ec << ", " << ec.message() << std::endl;
			return nullptr;
		}

		if (pkt.streamIndex() != m_videoInputStream.index()) {
			continue;
		}

		auto ts = pkt.ts();
		std::cout << "Read packet: " << ts << " / " << ts.seconds() << " / " << pkt.timeBase() << " / st: " << pkt.streamIndex() << std::endl;

		av::VideoFrame frame = m_videoCodecContext->decode(pkt, ec);

		//count++;
		//if (count > 100)
		//    break;

		if (ec) {
			std::cerr << "Error: " << ec << ", " << ec.message() << std::endl;
			return nullptr;
		} else if (!frame) {
			std::cerr << "Empty frame\n";
			//continue;
		}

		ts = frame.pts();

		std::cout << "  Frame: " << frame.width() << "x" << frame.height() << ", size=" << frame.size() << ", ts=" << ts << ", tm: " << ts.seconds() << ", tb: " << frame.timeBase() << ", ref=" << frame.isReferenced() << ":" << frame.refCount() << std::endl;

	}
#endif





	auto isFrameComplete = false;
	av::VideoFrame frame;
	auto t = std::chrono::high_resolution_clock::now();
	while(isFrameComplete == false)
	{
		std::error_code errCode;
		m_packet = m_formatContext.readPacket(errCode);
		check_error(errCode);

		if(m_packet.streamIndex() != m_videoInputStream.index())
			return ReadFrame(outPts);

		frame = m_videoCodecContext->decode(m_packet,errCode);
		check_error(errCode);
		isFrameComplete = frame.isComplete();
	}
	auto tDelta = std::chrono::high_resolution_clock::now() -t;
	//
	//frame.setTimeBase(encoder.timeBase());
	auto width = frame.width();
	auto height = frame.height();
	if(m_frame == nullptr)
	{
		m_frame = uimg::ImageBuffer::Create(width,height,uimg::ImageBuffer::Format::RGBA8);
		m_swsContext = sws_getContext(width,height,frame.pixelFormat().get(),width,height,AV_PIX_FMT_RGBA,SWS_BICUBIC,NULL,NULL,NULL);
		m_width = width;
		m_height = height;
	}
	if(width != m_frame->GetWidth() || height != m_frame->GetHeight())
		return nullptr;
	const std::array<uint8_t*,AV_NUM_DATA_POINTERS> srcFrameData = {
		frame.data(0),
		frame.data(1),
		frame.data(2),
		frame.data(3)
#if AV_NUM_DATA_POINTERS == 8
		,
		frame.data(4),
		frame.data(5),
		frame.data(6),
		frame.data(7)
#endif
	};
	const std::array<uint8_t*,1> dstFrameData = {
		static_cast<uint8_t*>(m_frame->GetData())
	};
	std::array<int,1> dstLineSize = {
		m_frame->GetWidth() *m_frame->GetPixelSize()
	};

	auto t2 = std::chrono::high_resolution_clock::now();
	sws_scale(m_swsContext,srcFrameData.data(),frame.raw()->linesize,0,frame.height(),dstFrameData.data(),dstLineSize.data());
	auto tDelta2 = std::chrono::high_resolution_clock::now() -t2;
	std::cout<<"Time passed: "<<(std::chrono::duration_cast<std::chrono::nanoseconds>(tDelta).count() /1'000'000'000.0)<<","<<(std::chrono::duration_cast<std::chrono::nanoseconds>(tDelta2).count() /1'000'000'000.0)<<std::endl;

	//auto ts = m_packet.ts();
	//std::cout << "Read packet: " << ts << " / " << ts.seconds() << " / " << m_packet.timeBase() << " / st: " << m_packet.streamIndex() << std::endl;

	//ts = frame.pts();
	//auto frameIndex = frame.raw()->display_picture_number;
	//std::cout << "  Frame: " << frame.width() << "x" << frame.height() << ", size=" << frame.size() << ", ts=" << ts << ", tm: " << ts.seconds() << ", tb: " << frame.timeBase() << ", ref=" << frame.isReferenced() << ":" << frame.refCount() << std::endl;
	//auto bestEffortTimestamp = av::frame::get_best_effort_timestamp(frame.raw());
	//auto time = bestEffortTimestamp /m_audioInputStream.timeBase().getDouble();

	int64_t pts = 0;
	if(m_packet.raw()->dts != AV_NOPTS_VALUE) {
		pts = av::frame::get_best_effort_timestamp(frame.raw());
	} else {
		pts = 0;
	}
	double test = pts *av_q2d(m_videoInputStream.raw()->time_base);

	outPts = test;

	//auto t = pts *av_q2d(m_videoInputStream.raw()->time_base);
	//outPts = t;
	return m_frame;
}
#pragma optimize("",on)
