#ifndef _cppCom_h__
#define _cppCom_h__

extern "C"
{
    #include "ffplayCom.h"
}


#include <memory>
#include "videoPackQue.h"
#include "cppClock.h"
#include "frameQue.h"

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
/*
class videoFrameQue:public frameQue
{
public:
    explicit videoFrameQue();
};
*/
struct globalData
{
    globalData ();
    cppDecoder    vidDec;
    cppDecoder    m_audDec;
    cppDecoder    m_subDec;
    videoPackQue  vidPackQ;
    packQue       m_audioPackQ;
    packQue       m_subPackQ;
    cppClock      vidClk;
    cppClock      m_audclk;

    frameQue      m_pictQ;
    frameQue      m_sampQ;
    frameQue      m_subpQ;
};

extern "C"
{
void cpp_decoder_destroy(cppDecoder& rD);
void cpp_check_external_clock_speed(VideoState *is);
void cpp_video_refresh(void *opaque, double *remaining_time);
double cpp_get_master_clock(VideoState *is);
// double cpp_compute_target_delay(double delay, VideoState *is);
int cpp_queue_picture(VideoState *is, AVFrame *src_frame, double pts, double duration, int64_t pos, int serial);
}

int cpp_decoder_decode_frame(cppDecoder& rD, AVFrame *frame, AVSubtitle *sub);
void cpp_sync_clock_to_slave(Clock *c, cppClock &rSlave);

#endif
