#include "globalData.h"
#include "strFun.h"


globalData::globalData ():m_audioPackQ(64), m_subPackQ(10), vidClk(&vidPackQ), m_audclk(&m_audioPackQ),m_extclk(nullptr), m_pictQ(16, true), m_sampQ(32 * 1, true), m_subpQ(8, false)
{
}
globalData:: ~globalData ()
{
}
int  globalData:: frame_drops_early()
{
    return m_frame_drops_early.load(std::memory_order_relaxed);
}

void  globalData:: setFrame_drops_early(int nI)
{
    m_frame_drops_early.store(nI, std::memory_order_relaxed);
}

int    globalData:: sample_array_index()
{
    return m_sample_array_index.load(std::memory_order_relaxed);
}

void   globalData:: setSample_array_index(int nI)
{
    m_sample_array_index.store(nI, std::memory_order_relaxed);
}

int16_t*   globalData:: sample_array()
{
    return m_sample_array;
}

int  globalData:: vfilterIdx()
{
    return m_vfilter_idx.load(std::memory_order_relaxed);
}

void  globalData::setVfilterIdx(int v)
{
    m_vfilter_idx.store(v, std::memory_order_relaxed);
}

int  globalData:: showMode()
{
    return m_show_mode.load(std::memory_order_relaxed);
}

void  globalData::setShowMode(int v)
{
    m_show_mode.store(v, std::memory_order_relaxed);
}

int  globalData:: audioVolume()
{
    return m_audioVolume.load(std::memory_order_relaxed);
}

void  globalData::setAudioVolume(int v)
{
    m_audioVolume.store(v, std::memory_order_relaxed);
}

int  globalData:: audio_write_buf_size()
{
    return m_audio_write_buf_size.load(std::memory_order_relaxed);
}

void  globalData::setAudio_write_buf_size(int v)
{
    m_audio_write_buf_size.store(v, std::memory_order_relaxed);
}

bool    globalData:: abort()
{
    return m_abort.load(std::memory_order_relaxed);
}

void    globalData :: setAbort(bool a)
{
    m_abort.store(a, std::memory_order_relaxed);
}

bool globalData::  muted()
{

    return m_muted.load(std::memory_order_relaxed);
}

void  globalData:: setMuted(bool a)
{
    m_muted.store(a, std::memory_order_relaxed);
}

bool    globalData:: paused()
{
    return m_paused.load(std::memory_order_relaxed);
}

void    globalData:: setPaused(bool b)
{
    m_paused.store(b, std::memory_order_relaxed);
}

double  globalData:: curPlayPos ()
{
    return m_curPlayPos.load(std::memory_order_relaxed);
}

void    globalData:: setCurPlayPos (double pos)
{
    m_curPlayPos.store(pos, std::memory_order_relaxed);
}

int  globalData:: videoStream ()
{
    return m_videoStream;
}

void  globalData:: setVideoStream (int v)
{
    m_videoStream = v;
}

int  globalData:: audioStream ()
{
    return m_audioStream;
}

void  globalData:: setAudioStream (int v)
{
    m_audioStream = v;
}

int  globalData:: subtitleStream ()
{
    return m_subtitleStream;
}

void  globalData:: setSubtitleStream (int v)
{
    m_subtitleStream = v;
}

AVStream*  globalData:: videoSt ()
{
    return m_videoSt;
}

void globalData:: setVideoSt (AVStream* v)
{
    m_videoSt = v;
}

AVStream*  globalData::audioSt ()
{
    return m_audio_st;
}

void  globalData::setAudioSt (AVStream* v)
{
    m_audio_st = v;
}

int  globalData:: av_sync_type()
{
    return m_av_sync_type;
}

int  globalData:: realtime ()
{
    return m_realtime;
}

void  globalData:: setRealtime (int v)
{
    m_realtime = v;
}
