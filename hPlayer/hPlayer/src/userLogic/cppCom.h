#ifndef _cppCom_h__
#define _cppCom_h__

extern "C"
{
    #include "ffplayCom.h"
}


#include <memory>
#include "pSPSCQueue.h"
#include "videoPackQue.h"
#include "cppClock.h"

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

struct globalData
{
    globalData ();
    cppDecoder    vidDec;
    videoPackQue  vidPackQ;
    cppClock      vidClk;
};

extern "C"
{
void cpp_decoder_destroy(cppDecoder& rD);
void cpp_check_external_clock_speed(VideoState *is);
void cpp_video_refresh(void *opaque, double *remaining_time);
double cpp_get_master_clock(VideoState *is);
double cpp_compute_target_delay(double delay, VideoState *is);
}
#endif
