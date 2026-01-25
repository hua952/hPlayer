#ifndef _ffmpegDecoder_h__
#define _ffmpegDecoder_h__
#include <memory>
#include <vector>
#include <stdexcept>
#include <thread>
#include <mutex>
#include <queue>
#include <chrono>
#include <optional>
#include <set>
#include <algorithm>
#include <atomic>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/channel_layout.h>
#include <libavutil/imgutils.h>
}
#include "comman.h"

// ==================== Global Audio/Video State ====================
/*
struct DecodedFrame {
    AVFrame* frame = nullptr;
    int64_t pts = 0;
    double pts_seconds = 0.0; // added: presentation time in seconds
};
*/
enum playState
{
    playState_noFile,
    playState_playing,
    playState_End,
};
class decoTh;
class ffmpegDecoder
{
public:
    ffmpegDecoder (decoTh& rWorn);
    ~ffmpegDecoder ();
    int init ();
    int playFile(const char* file);
    void  pushVideo(const VideoFrame& df);
    bool popVideo(VideoFrame& df);
    void cleanVideo();
    void cleanAideo();
    void  pushAudio (uint8_t* pData, int dataSize);
    bool  popAudio (unsigned char* stream, int len);
    double audioClockSec ();
    void cleanup();
    int onLoopFrame();

    int g_audioChannels = 2;
    int g_audioSampleRate = 48000;
    bool g_quit = false;
    playState  state ();
    void       setState(playState st);
    double  totalDuration ();
    void    setTotalDuration (double dur);
    double  getSeekRequest();
    void    setSeekRequest(double dv);
    void seekTo(double sec);
    std::atomic<int64_t> g_audioPlayedSamples{ 0 };
    int video_stream_idx();
    int audio_stream_idx();

    decoTh& worn();
private:
    decoTh& m_rWorn;
    int m_video_stream_idx = -1;
    int m_audio_stream_idx = -1;
    AVFormatContext* m_fmt = nullptr;
    AVCodecContext* m_vdec_ctx = nullptr;
    AVCodecContext* m_adec_ctx = nullptr;
    SwrContext* m_swr_ctx = nullptr;
    struct SwsContext* m_sws_ctx = nullptr;
    AVPacket* m_pkt = nullptr;
    AVFrame* m_frame = nullptr;
    AVFrame* m_conv_frame = nullptr;

    double  m_totalDuration = 1;
    playState  m_state = playState_noFile;
    std::queue<VideoFrame> g_videoQueue;
    std::mutex g_videoMutex;
    std::condition_variable g_videoCV;

    std::queue<std::vector<uint8_t>> g_audioQueue;
    std::mutex g_audioMutex;
    // audio clock: number of audio samples already played (per channel)
    std::atomic<double> m_seekRequest{NAN}; 
private:
    void cleanPlayRes();
};
#endif
