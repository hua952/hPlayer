#ifndef _cppCom_h__
#define _cppCom_h__

#include "globalData.h"

/*
class videoFrameQue:public frameQue
{
public:
    explicit videoFrameQue();
};
*/


extern "C"
{
void cpp_decoder_destroy(cppDecoder& rD);
void cpp_check_external_clock_speed(VideoState *is);
/*void cpp_video_refresh(void *opaque, double *remaining_time);*/
double cpp_get_master_clock(VideoState *is);
/* double cpp_compute_target_delay(double delay, VideoState *is);*/
int cpp_queue_picture(VideoState *is, AVFrame *src_frame, double pts, double duration, int64_t pos, int serial);
}

int cpp_decoder_decode_frame(cppDecoder& rD, AVFrame *frame, AVSubtitle *sub);
void cpp_sync_clock_to_slave(cppClock&c, cppClock &rSlave);

int cpp_get_master_sync_type(VideoState *is);

int create_hwaccel(AVBufferRef **device_ctx);

int cpp_decoder_init(cppDecoder& rD, AVCodecContext *avctx, packQue* queue);
#endif
