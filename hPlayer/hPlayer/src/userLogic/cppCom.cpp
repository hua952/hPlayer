#include "cppCom.h"
#include "cppClock.h"

#include "tSingleton.h"

extern "C"
{
int64_t cpp_frame_queue_last_pos(FrameQueue *f)
{
    auto& rGlobal = tSingleton<globalData>::single();
    auto& rVidClk = rGlobal.vidClk;

    Frame *fp = &f->queue[f->rindex];
    if (f->rindex_shown && fp->serial == rVidClk.serial())
        return fp->pos;
    else
        return -1;
}


Frame* cpp_frame_queue_peek_writable(FrameQueue *f)
{
    /* wait until we have space to put a new frame */
    SDL_LockMutex(f->mutex);
    while (f->size >= f->max_size/* &&
           !f->pktq->abort_request*/) {
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

double cpp_compute_target_delay(double delay, VideoState *is)
{
    double sync_threshold, diff = 0;
    auto& rGlobal = tSingleton<globalData>::single();
    auto& rVidClk = rGlobal.vidClk;

    /* update delay to follow master synchronisation source */
    if (get_master_sync_type(is) != AV_SYNC_VIDEO_MASTER) {
        /* if video is slave, we try to correct big delays by
           duplicating or deleting a frame */
        // diff = get_clock(&is->vidclk) - cpp_get_master_clock(is);
        diff = rVidClk.getClock() - cpp_get_master_clock(is);

        /* skip or repeat frame. We take into account the
           delay to compute the threshold. I still don't know
           if it is the best guess */
        sync_threshold = FFMAX(AV_SYNC_THRESHOLD_MIN, FFMIN(AV_SYNC_THRESHOLD_MAX, delay));
        if (!isnan(diff) && fabs(diff) < is->max_frame_duration) {
            if (diff <= -sync_threshold)
                delay = FFMAX(0, delay + diff);
            else if (diff >= sync_threshold && delay > AV_SYNC_FRAMEDUP_THRESHOLD)
                delay = delay + diff;
            else if (diff >= sync_threshold)
                delay = 2 * delay;
        }
    }

    av_log(NULL, AV_LOG_TRACE, "video: delay=%0.3f A-V=%f\n",
            delay, -diff);

    return delay;
}
}

globalData::globalData ():vidClk(vidPackQ)
{
}

