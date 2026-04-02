#include "cppCom.h"
#include "cppClock.h"

#include "tSingleton.h"

extern "C"
{
int64_t cpp_video_frame_queue_last_pos()
{
    auto& rGlobal = tSingleton<globalData>::single();
    auto& rVidClk = rGlobal.vidClk;
    auto& rPictQ = rGlobal.m_pictQ;
    auto nRet = -1;
    do {
        if (!rPictQ.haveLastFrame()) {
            break;
        }
        auto fp = rPictQ.lastFrame();
        if (fp->serial != rVidClk.serial()) {
            break;
        }
        nRet = fp->pos;
    } while (0);
    return nRet;
}

/*
cppFrame* cpp_frame_queue_peek_writable(FrameQueue *f)
{
    SDL_LockMutex(f->mutex);
    while (f->size >= f->max_size &&
           !f->pktq->abort_request) {
        SDL_CondWait(f->cond, f->mutex);
    }
    SDL_UnlockMutex(f->mutex);

    auto& rGlobal = tSingleton<globalData>::single();
    auto& rVidPackQ = rGlobal.vidPackQ;
    if (rVidPackQ.abort_request()) {
        return NULL;
    }

    return &f->queue[f->windex];
}
*/
int cpp_queue_picture(VideoState *is, AVFrame *src_frame, double pts, double duration, int64_t pos, int serial)
{
    cppFrame *vp = nullptr;
    auto& rGlobal = tSingleton<globalData>::single();
    auto& rPictQ = rGlobal.m_pictQ;
#if defined(DEBUG_SYNC)
    printf("frame_type=%c pts=%0.3f\n",
           av_get_picture_type_char(src_frame->pict_type), pts);
#endif

    if (!(vp = rPictQ.nextWrite()))
        return -1;

    vp->sar = src_frame->sample_aspect_ratio;
    vp->uploaded = 0;

    vp->width = src_frame->width;
    vp->height = src_frame->height;
    vp->format = src_frame->format;

    vp->pts = pts;
    vp->duration = duration;
    vp->pos = pos;
    vp->serial = serial;

    set_default_window_size(vp->width, vp->height, vp->sar);

    av_frame_move_ref(vp->frame, src_frame);
    // frame_queue_push(&is->pictq);
    rPictQ.push();
    return 0;
}

double cpp_get_master_clock(VideoState *is)
{
    double val;

    auto& rGlobal = tSingleton<globalData>::single();
    auto& rVidClk = rGlobal.vidClk;

    switch (get_master_sync_type(is)) {
        case AV_SYNC_VIDEO_MASTER:
            val = rVidClk.getClock();
            break;
        case AV_SYNC_AUDIO_MASTER:
            val = get_clock(&is->audclk);
            break;
        default:
            val = get_clock(&is->extclk);
            break;
    }
    return val;
}


}

globalData::globalData ():vidClk(vidPackQ), m_pictQ(8, true)
{
}

