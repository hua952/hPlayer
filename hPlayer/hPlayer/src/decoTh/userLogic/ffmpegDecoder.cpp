#include "ffmpegDecoder.h"
#include <string>
#include <iostream>
#include <SDL2/SDL.h>
#include "gLog.h"

//#include "playerDataRpc.h"
#include "decoTh.h"
#include "msgGroupId.h"
#include "playerDataMsgId.h"
#include "loopHandleS.h"
#include "globalData.h"
char av_error[1024] = { 0 };
#define av_errStr(errnum) av_make_error_string(av_error, AV_ERROR_MAX_STRING_SIZE, errnum)

ffmpegDecoder:: ffmpegDecoder (decoTh& rWorn):m_rWorn(rWorn)
{
}

ffmpegDecoder:: ~ffmpegDecoder ()
{
}

bool   ffmpegDecoder:: popAudio (unsigned char* stream, int len)
{
    bool   nRet = true;
    int remaining = len;
    {
    std::lock_guard<std::mutex> lock(g_audioMutex);
    while (remaining > 0 && !g_audioQueue.empty()) {
        auto& buf = g_audioQueue.front();
        int toCopy = std::min(remaining, (int)buf.size());
        memcpy(stream, buf.data(), toCopy);
        // update audio clock: S16 stereo -> bytesPerSampleAllChannels = 2 * channels
        int bytesPerSampleAllChannels = g_audioChannels * sizeof(int16_t);
        int samplesConsumed = toCopy / bytesPerSampleAllChannels;
        if (samplesConsumed > 0) {
            g_audioPlayedSamples.fetch_add(samplesConsumed, std::memory_order_relaxed);
        }
        stream += toCopy;
        remaining -= toCopy;
        buf.erase(buf.begin(), buf.begin() + toCopy);
        if (buf.empty()) g_audioQueue.pop();
    }
    }
    memset(stream, 0, remaining);
    return nRet;
}

static std::string av_err2str_wrap(int err) {
    char buf[256] = { 0 };
    av_strerror(err, buf, sizeof(buf));
    return std::string(buf);
}

// ==================== SDL Audio Callback ====================
void SDLCALL audioCallback(void* userdata, Uint8* stream, int len) {
    auto  pThis = (ffmpegDecoder*) userdata;
    pThis->popAudio (stream, len);
}

int  ffmpegDecoder:: init ()
{
    int  nRet = 0;
    do {
        // SDL_Init(SDL_INIT_AUDIO);
        SDL_AudioSpec want{};
        want.freq = 48000;
        want.format = AUDIO_S16SYS;
        want.channels = 2;
        want.samples = 1024;
        want.callback = audioCallback;
        want.userdata = this;
        if (SDL_OpenAudio(&want, nullptr) < 0) {
            gError("SDL_OpenAudio failed: " << SDL_GetError());
            nRet = 1;
            break;
        }
        SDL_PauseAudio(0);
    } while (0);
    return nRet;
}

decoTh&  ffmpegDecoder:: worn()
{
    return m_rWorn;
}

static inline bool is_avpacket_unrefed(const AVPacket* pkt) {
    // 1. 空指针直接视为“已unref”
    if (!pkt) {
        return true;
    }
    // 2. 核心判断：无缓冲区引用 + 无数据
    //    buf==NULL：无引用计数的缓冲区（unref后必为NULL）
    //    data==NULL && size==0：无原始数据（unref后必满足）
    if (pkt->buf == NULL && pkt->data == NULL && pkt->size == 0) {
        return true;
    }
    // 3. 补充判断：即使有数据但size=0，也视为空包（防御性判断）
    if (pkt->size <= 0) {
        return true;
    }
    // 4. 有有效数据
    return false;
}

int  ffmpegDecoder:: onLoopFrame()
{
    int  nRet = 0;
    do {
        if (playState_playing != state()) {
            break;
        }
        /*
        double seekTarget = pThis->getSeekRequest();
        if (!std::isnan(seekTarget)) {
            // int64_t targetTs = static_cast<int64_t>(seekTarget / av_q2d(videoStream->time_base));
            av_seek_frame(fmt, -1, (int64_t)(seekTarget * AV_TIME_BASE), AVSEEK_FLAG_BACKWARD);
            avcodec_flush_buffers(vdec_ctx); // 清理解码器缓冲
            avcodec_flush_buffers(adec_ctx); // 清理解码器缓冲
            pThis->cleanVideo();
            pThis->cleanAideo();
            auto temp = seekTarget * pThis->g_audioSampleRate;
            auto curA =  static_cast<int64_t>(temp);
            pThis->g_audioPlayedSamples.store(curA, std::memory_order_relaxed);
            pThis->setSeekRequest(NAN);
        }
        */
        if (is_avpacket_unrefed(m_pkt)) {
            auto ret = av_read_frame(m_fmt, m_pkt);
            if (ret < 0){
                setState(playState_End);
                break;
            }
        }

        auto& wor = worn();
        if (m_pkt->stream_index == m_video_stream_idx && m_vdec_ctx) {
            auto& que = getDecodeRenderQueue();
            // 先检查队列是否有剩余容量（非100%准确，但能大幅减少无效构造）
            if (que.mabeFull()) {
                break;
            }

            avcodec_send_packet(m_vdec_ctx, m_pkt);
            while (avcodec_receive_frame(m_vdec_ctx, m_frame) == 0) {
                int w = m_frame->width;
                int h = m_frame->height;
                // Convert to NV12
                m_sws_ctx = sws_getCachedContext(m_sws_ctx,
                    w, h, (AVPixelFormat)m_frame->format,
                    w, h, AV_PIX_FMT_NV12,
                    SWS_BILINEAR, nullptr, nullptr, nullptr);
                if (!m_sws_ctx) {
                    std::cerr << "sws_getCachedContext failed\n";
                    av_frame_unref(m_frame);
                    continue;
                }
                int bufsize = av_image_get_buffer_size(AV_PIX_FMT_NV12, w, h, 1);
                std::vector<uint8_t> buffer(bufsize);
                av_image_fill_arrays(m_conv_frame->data, m_conv_frame->linesize, buffer.data(), AV_PIX_FMT_NV12, w, h, 1);
                sws_scale(m_sws_ctx, m_frame->data, m_frame->linesize, 0, h, m_conv_frame->data, m_conv_frame->linesize);
                int y_linesize = m_conv_frame->linesize[0];
                udword yPkaneSize = (udword)y_linesize * h;
                
                int uv_h = (h + 1) / 2;
                int uv_linesize = m_conv_frame->linesize[1];
                udword planeSize = (udword)uv_linesize * uv_h;
                planeSize += yPkaneSize;
                
                auto pPlanPt = std::make_unique<ubyte[]> (planeSize);
                auto pPlanData = pPlanPt.get();
                for (int row = 0; row < h; ++row) {
                    memcpy(pPlanData + (size_t)row * y_linesize, m_conv_frame->data[0] + (size_t)row * y_linesize, y_linesize);
                }
                auto pUVPlaneData = pPlanData + yPkaneSize;
                for (int row = 0; row < uv_h; ++row) {
                    memcpy(pUVPlaneData + (size_t)row * uv_linesize, m_conv_frame->data[1] + (size_t)row * uv_linesize, uv_linesize);
                }
                int64_t pts = m_frame->pts;
                if (pts == AV_NOPTS_VALUE) pts = m_frame->best_effort_timestamp;
                if (pts == AV_NOPTS_VALUE) pts = 0;
                AVRational tb = m_fmt->streams[m_video_stream_idx]->time_base;
                
                auto bRet = que.try_emplace(videoFrameData{
                        .m_pts_seconds = pts * av_q2d(tb),
                        .m_width = (udword)w,
                        .m_height = (udword)h,
                        .m_y_linesize = (udword)y_linesize,
                        .m_uv_linesize = (udword)uv_linesize,
                        .m_y_planesize = yPkaneSize,
                        .m_planNum = planeSize,
                        .m_plan = std::move(pPlanPt)
                        });
                if (!bRet) {
                    gWarn("tryEmplaceVideoFrame ret false");
                }
                av_frame_unref(m_frame);
            }
        }
        else if (m_pkt->stream_index == m_audio_stream_idx && m_adec_ctx && m_swr_ctx) {
            avcodec_send_packet(m_adec_ctx, m_pkt);
            while (avcodec_receive_frame(m_adec_ctx, m_frame) == 0) {
                int64_t delay = swr_get_delay(m_swr_ctx, m_frame->sample_rate);
                int dst_nb_samples = av_rescale_rnd(delay + m_frame->nb_samples, g_audioSampleRate,  m_frame->sample_rate, AV_ROUND_UP);
                uint8_t* outbuf = (uint8_t*)av_malloc(dst_nb_samples * 2 * 2);
                if (!outbuf) {
                    av_frame_unref(m_frame);
                    continue;
                }
                int nb = swr_convert(m_swr_ctx, &outbuf, dst_nb_samples, (const uint8_t**)m_frame->data, m_frame->nb_samples);
                if (nb > 0) {
                    int out_bytes = nb * 2 * 2;
                    out_bytes = (out_bytes / 4) * 4;
                    if (out_bytes >= 4) {
                        pushAudio (outbuf, out_bytes);
                    }
                }
                av_freep(&outbuf);
                av_frame_unref(m_frame);
            }
        }
        av_packet_unref(m_pkt);
    } while (0);
    return nRet;
}

void  ffmpegDecoder:: cleanPlayRes()
{
    do {
        if (m_sws_ctx){
            sws_freeContext(m_sws_ctx);
            m_sws_ctx = nullptr;
        }
        if (m_swr_ctx) swr_free(&m_swr_ctx);
        if (m_vdec_ctx) avcodec_free_context(&m_vdec_ctx);
        if (m_adec_ctx) avcodec_free_context(&m_adec_ctx);
        avformat_close_input(&m_fmt);
    } while (0);
}

int  ffmpegDecoder:: video_stream_idx()
{
    return m_video_stream_idx;
}

int  ffmpegDecoder:: audio_stream_idx()
{
    return m_audio_stream_idx;
}

int  ffmpegDecoder:: playFile(const char* filename)
{
    int  nRet = 0;
    do {
        cleanPlayRes();
        g_audioPlayedSamples.store(0, std::memory_order_relaxed);
        int ret = avformat_open_input(&m_fmt, filename, nullptr, nullptr);
        if (ret < 0) {
            gError("avformat_open_input failed: " << av_err2str_wrap(ret));
            nRet = 1;
            break;
        }
        auto fmt = m_fmt;
        if ((ret = avformat_find_stream_info(fmt, nullptr)) < 0) {
            gError("avformat_find_stream_info failed: " << av_err2str_wrap(ret));
            nRet = 2;
            break;
        }

        m_video_stream_idx = -1;
        m_audio_stream_idx = -1;
        for (unsigned i = 0; i < fmt->nb_streams; ++i) {
            if (fmt->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && m_video_stream_idx < 0) m_video_stream_idx = i;
            if (fmt->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && m_audio_stream_idx < 0) m_audio_stream_idx= i;
        }
        setTotalDuration(static_cast<double>(fmt->duration) / AV_TIME_BASE);
        setState(playState_playing);


        AVChannelLayout src_ch_layout = { 0 };
        AVChannelLayout dst_ch_layout = AV_CHANNEL_LAYOUT_STEREO;
        if (m_video_stream_idx >= 0) {
            const AVCodec* vcodec = avcodec_find_decoder(fmt->streams[m_video_stream_idx]->codecpar->codec_id);
            if (!vcodec) {
                gWarn("video codec not found");
            }
            else {
                m_vdec_ctx = avcodec_alloc_context3(vcodec);
                avcodec_parameters_to_context(m_vdec_ctx, fmt->streams[m_video_stream_idx]->codecpar);
                if ((ret = avcodec_open2(m_vdec_ctx, vcodec, nullptr)) < 0) {
                    gError("avcodec_open2(video) failed: " << av_err2str_wrap(ret));
                    nRet = 3;
                    break;
                }
            }
        }
        if (m_audio_stream_idx >= 0) {
            const AVCodec* acodec = avcodec_find_decoder(fmt->streams[m_audio_stream_idx]->codecpar->codec_id);
            if (!acodec) {
                gWarn("Unsupported audio codec");
            }
            else {
                m_adec_ctx = avcodec_alloc_context3(acodec);
                avcodec_parameters_to_context(m_adec_ctx, fmt->streams[m_audio_stream_idx]->codecpar);
                if (avcodec_open2(m_adec_ctx, acodec, nullptr) < 0) {
                    gWarn("Failed to open audio decoder");
                }
                else {
                    if (m_adec_ctx->ch_layout.nb_channels > 0) {
                        av_channel_layout_copy(&src_ch_layout, &m_adec_ctx->ch_layout);
                    }
                    else {
                        av_channel_layout_from_mask(&src_ch_layout, AV_CH_LAYOUT_STEREO);
                    }
                    int ret = swr_alloc_set_opts2(
                            &m_swr_ctx,
                            &dst_ch_layout, AV_SAMPLE_FMT_S16, g_audioSampleRate,
                            &src_ch_layout, m_adec_ctx->sample_fmt, m_adec_ctx->sample_rate,
                            0, nullptr
                            );
                    if (ret < 0 || !m_swr_ctx) {
                        char errbuf[256];
                        av_strerror(ret, errbuf, sizeof(errbuf));
                        gWarn("swr_alloc_set_opts2 failed: " << errbuf);
                        av_channel_layout_uninit(&src_ch_layout);
                        av_channel_layout_uninit(&dst_ch_layout);
                    }
                    else {
                        if (swr_init(m_swr_ctx) < 0) {
                            gWarn("swr_init failed");
                            swr_free(&m_swr_ctx);
                            m_swr_ctx = nullptr;
                        }
                    }
                }
            }
        }

        m_pkt = av_packet_alloc();
        m_frame = av_frame_alloc();
        m_conv_frame = av_frame_alloc();
    } while (0);
    if (nRet) {
        cleanPlayRes();
    }
    return nRet;
}

void   ffmpegDecoder:: pushAudio (uint8_t* pData, int dataSize)
{
    std::vector<uint8_t> audioData(pData, pData + dataSize);
    {
        std::lock_guard<std::mutex> lock(g_audioMutex);
        g_audioQueue.push(std::move(audioData));
    }
}

void  ffmpegDecoder:: cleanAideo()
{
    decltype(g_audioQueue) temp;
    {
        std::lock_guard<std::mutex> lock(g_audioMutex);
        std::swap(g_audioQueue, temp);
    }
}

void  ffmpegDecoder:: cleanVideo()
{
    decltype(g_videoQueue) temp;
    {
        std::lock_guard<std::mutex> lock(g_videoMutex);
        std::swap(g_videoQueue, temp);
    }
}

void   ffmpegDecoder:: pushVideo(const VideoFrame& df)
{
    {
        std::lock_guard<std::mutex> lock(g_videoMutex);
        g_videoQueue.push(df);
        g_videoCV.notify_one();
    }

}

bool ffmpegDecoder::popVideo(VideoFrame& df)
{
    bool  nRet = true;
     std::lock_guard<std::mutex> lock(g_videoMutex);
    if (g_videoQueue.empty()) {
        return false;
    } else {
        df = std::move(g_videoQueue.front());
        g_videoQueue.pop();
        return true;
    }
}

double  ffmpegDecoder:: audioClockSec ()
{
    return (double)g_audioPlayedSamples.load(std::memory_order_relaxed) / (double)g_audioSampleRate;
}

void  ffmpegDecoder:: cleanup()
{
    cleanPlayRes ();
    SDL_CloseAudio();
}

playState  ffmpegDecoder:: state ()
{
    return m_state;
}

void ffmpegDecoder:: setState(playState st)
{
    m_state = st;
}

double   ffmpegDecoder:: totalDuration ()
{
    double   nRet = 1;
    do {
        if (playState_playing == m_state) {
            nRet = m_totalDuration;
        }
    } while (0);
    return nRet;
}

void     ffmpegDecoder:: setTotalDuration (double dur)
{
    do {
        m_totalDuration = dur;
    } while (0);
}

void  ffmpegDecoder:: seekTo(double sec)
{
    setSeekRequest(sec);
}

double   ffmpegDecoder:: getSeekRequest()
{
    return m_seekRequest.load(std::memory_order_relaxed);
}

void ffmpegDecoder:: setSeekRequest(double dv)
{
    m_seekRequest.store(dv, std::memory_order_relaxed);
}

