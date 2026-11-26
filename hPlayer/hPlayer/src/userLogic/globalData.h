#ifndef _globalData_h__
#define _globalData_h__
#include <memory>
extern "C"
{
    #include "ffplayCom.h"
}


#include "videoPackQue.h"
#include "cppClock.h"
#include "frameQue.h"

#include <atomic>

struct cppDecoder {
    AVPacket *pkt;
    packQue* queue;
    AVCodecContext *avctx;
    int pkt_serial;
    int finished;
    int packet_pending;
    int64_t start_pts;
    AVRational start_pts_tb;
    int64_t next_pts;
    AVRational next_pts_tb;
};
class globalData
{
public:
    globalData ();
    ~globalData ();
    cppDecoder    vidDec;
    cppDecoder    m_audDec;
    cppDecoder    m_subDec;
    videoPackQue  vidPackQ;
    packQue       m_audioPackQ;
    packQue       m_subPackQ;
    cppClock      vidClk;
    cppClock      m_audclk;
    cppClock      m_extclk;
    frameQue      m_pictQ;
    frameQue      m_sampQ;
    frameQue      m_subpQ;

    bool   muted();
    void   setMuted(bool a);
    bool   abort();
    void   setAbort(bool a);
    bool   paused();
    void   setPaused(bool b);
    double curPlayPos ();
    void   setCurPlayPos (double pos);
    int  videoStream ();
    void  setVideoStream (int v);
    int  audioStream ();
    void  setAudioStream (int v);
    int  subtitleStream ();
    void  setSubtitleStream (int v);
    AVStream*  videoSt ();
    void  setVideoSt (AVStream* v);
    AVStream*  audioSt ();
    void  setAudioSt (AVStream* v);
    int   showMode();
    void  setShowMode(int v);
    int   vfilterIdx();
    void  setVfilterIdx(int v);
    int   sample_array_index();
    void  setSample_array_index(int nI);
    int16_t*  sample_array();
    int   frame_drops_early();
    void   setFrame_drops_early(int nI);
    int   audioVolume();
    void  setAudioVolume(int v);
    int   audio_write_buf_size();
    void  setAudio_write_buf_size(int v);
    int   av_sync_type();
    int  realtime ();
    void  setRealtime (int v);
    const char*  fileName ();
    void  setFileName (const char* v);
private:
    std::unique_ptr<char[]>  m_fileName;
    int  m_realtime;
    int16_t m_sample_array[SAMPLE_ARRAY_SIZE];
    AVStream*  m_videoSt{nullptr};
    AVStream*  m_audio_st{nullptr};
    int  m_subtitleStream{-1};
    int  m_audioStream{-1};
    int  m_videoStream{-1};
    int  m_av_sync_type{AV_SYNC_AUDIO_MASTER};
    std::atomic<int> m_audio_write_buf_size{0};
    std::atomic<int> m_audioVolume{100};
    std::atomic<int> m_frame_drops_early{0};
    std::atomic<int> m_sample_array_index{0};
    std::atomic<int> m_vfilter_idx{0};
    std::atomic<int> m_show_mode{-1};
    std::atomic<bool> m_paused{false};
    std::atomic<bool> m_muted{false};
    std::atomic<bool> m_abort{false};
    std::atomic<double> m_curPlayPos;
};
#endif
