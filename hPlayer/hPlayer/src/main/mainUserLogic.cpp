#include "main.h"
#include "mainUserLogic.h"
#include "playerDataRpc.h"


#include "cppCom.h"
#include "cppClock.h"

#include "tSingleton.h"

extern "C"
{

void cpp_stream_toggle_pause(VideoState *is)
{

    auto& rGlobal = tSingleton<globalData>::single();
    // auto& rVidPackQ = rGlobal.vidPackQ;
    auto& rVidClk = rGlobal.vidClk;
    if (is->paused) {
        // is->frame_timer += av_gettime_relative() / 1000000.0 - is->vidclk.last_updated;
        is->frame_timer += av_gettime_relative() / 1000000.0 - rVidClk.lastUpdated();
        if (is->read_pause_return != AVERROR(ENOSYS)) {
            // is->vidclk.paused = 0;
            rVidClk.setPaused(0);
        }
        // set_clock(&is->vidclk, get_clock(&is->vidclk), is->vidclk.serial);
        rVidClk.setClock(rVidClk.getClock(), rVidClk.serial());
    }
    //set_clock(&is->extclk, get_clock(&is->extclk), is->extclk.serial);
    set_clock(&is->extclk, get_clock(&is->extclk), is->extclk.serial);
    is->paused = is->audclk.paused =  is->extclk.paused = !is->paused;
    rVidClk.setPaused(is->paused);
}

}


mainUserLogic::mainUserLogic (main& rServer):m_rmain(rServer)
{

}

int mainUserLogic::onLoopBegin()
{
    int nRet = 0;
    do {
        auto is = getVideoState();
        if (!is) [[unlikely]]{
            nRet = procPacketFunRetType_exitAfterLoop;
            break;
        }
        mainInitOKAskMsg msg;
        getServer().sendMsg(msg);

    } while (0);
    return nRet;
}

int mainUserLogic::onLoopFrame()
{
    int nRet = 0;
    do {
        if (mainState_willExit== m_mainState) [[unlikely]]{
            nRet = procPacketFunRetType_exitAfterLoop;
            break;
        }
        auto nR = ffplay_event_loop (getVideoState());
        if (nR) {
            readPackExitNtfAskMsg msg;
            getServer().sendMsg(msg);

            // setState(mainUserLogic::mainState_willExit);
            // getServer().ntfOtherLocalServerExit();
        }
    } while (0);
    return nRet;
}

int mainUserLogic::onLoopEnd()
{
    int nRet = 0;
    do {
    } while (0);
    return nRet;
}

mainUserLogic::mainState  mainUserLogic:: state()
{
    return m_mainState;
}

void   mainUserLogic:: setState(mainState st)
{
    m_mainState = st;
}

int  mainUserLogic:: initThis()
{
    int  nRet = 0;
    do {
    } while (0);
    return nRet;
}

void        mainUserLogic:: clean()
{
    do {
    } while (0);
}

static double compute_target_delay(double delay, VideoState *is)
{
    double sync_threshold, diff = 0;

    auto& rGlobal = tSingleton<globalData>::single();
    auto& rVidClk = rGlobal.vidClk;
    /* update delay to follow master synchronisation source */
    if (get_master_sync_type(is) != AV_SYNC_VIDEO_MASTER) {
        /* if video is slave, we try to correct big delays by
           duplicating or deleting a frame */
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

static void cpp_sync_clock_to_slave(Clock *c, cppClock &rSlave)
{
    double clock = get_clock(c);
    double slave_clock = rSlave.getClock();
    if (!isnan(slave_clock) && (isnan(clock) || fabs(clock - slave_clock) > AV_NOSYNC_THRESHOLD))
        set_clock(c, slave_clock, rSlave.serial());
}

static void update_video_pts(VideoState *is, double pts, int serial)
{
    /* update current video pts */
    auto& rGlobal = tSingleton<globalData>::single();
    auto& rVidClk = rGlobal.vidClk;
    rVidClk.setClock(pts, serial);
    cpp_sync_clock_to_slave(&is->extclk, rVidClk);
}

extern "C"
{
void cpp_video_refresh(void *opaque, double *remaining_time)
{
    VideoState *is = (VideoState *)opaque;
    double time;

    auto& rGlobal = tSingleton<globalData>::single();
    // auto& rDecoder = rGlobal.vidDec;
    auto& rVidPackQ = rGlobal.vidPackQ;
    auto& rVidClk = rGlobal.vidClk;

    Frame *sp, *sp2;

    if (!is->paused && get_master_sync_type(is) == AV_SYNC_EXTERNAL_CLOCK && is->realtime) {
        /*check_external_clock_speed(is);*/
        cpp_check_external_clock_speed(is);
    }

    if (!display_disable && is->show_mode != VideoState::SHOW_MODE_VIDEO && is->audio_st) {
        time = av_gettime_relative() / 1000000.0;
        if (is->force_refresh || is->last_vis_time + rdftspeed < time) {
            video_display(is);
            is->last_vis_time = time;
        }
        *remaining_time = FFMIN(*remaining_time, is->last_vis_time + rdftspeed - time);
    }

    if (is->video_st) {
retry:
        if (frame_queue_nb_remaining(&is->pictq) == 0) {
            // nothing to do, no picture to display in the queue
        } else {
            double last_duration, duration, delay;
            Frame *vp, *lastvp;

            /* dequeue the picture */
            lastvp = frame_queue_peek_last(&is->pictq);
            vp = frame_queue_peek(&is->pictq);

            if (vp->serial != rVidPackQ.serial()) {
                frame_queue_next(&is->pictq);
                goto retry;
            }

            if (lastvp->serial != vp->serial)
                is->frame_timer = av_gettime_relative() / 1000000.0;

            if (is->paused)
                goto display;

            /* compute nominal last_duration */
            last_duration = vp_duration(is, lastvp, vp);
            delay = cpp_compute_target_delay(last_duration, is);

            time= av_gettime_relative()/1000000.0;
            if (time < is->frame_timer + delay) {
                *remaining_time = FFMIN(is->frame_timer + delay - time, *remaining_time);
                goto display;
            }

            is->frame_timer += delay;
            if (delay > 0 && time - is->frame_timer > AV_SYNC_THRESHOLD_MAX)
                is->frame_timer = time;

            SDL_LockMutex(is->pictq.mutex);
            if (!isnan(vp->pts))
                update_video_pts(is, vp->pts, vp->serial);
            SDL_UnlockMutex(is->pictq.mutex);

            if (frame_queue_nb_remaining(&is->pictq) > 1) {
                Frame *nextvp = frame_queue_peek_next(&is->pictq);
                duration = vp_duration(is, vp, nextvp);
                if(!is->step && (framedrop>0 || (framedrop && get_master_sync_type(is) != AV_SYNC_VIDEO_MASTER)) && time > is->frame_timer + duration){
                    is->frame_drops_late++;
                    frame_queue_next(&is->pictq);
                    goto retry;
                }
            }

            if (is->subtitle_st) {
                while (frame_queue_nb_remaining(&is->subpq) > 0) {
                    sp = frame_queue_peek(&is->subpq);

                    if (frame_queue_nb_remaining(&is->subpq) > 1)
                        sp2 = frame_queue_peek_next(&is->subpq);
                    else
                        sp2 = NULL;

                    if (sp->serial != is->subtitleq.serial
                            // || (is->vidclk.pts > (sp->pts + ((float) sp->sub.end_display_time / 1000)))
                            // || (sp2 && is->vidclk.pts > (sp2->pts + ((float) sp2->sub.start_display_time / 1000))))
                            || (rVidClk.pts() > (sp->pts + ((float) sp->sub.end_display_time / 1000)))
                            || (sp2 && rVidClk.pts() > (sp2->pts + ((float) sp2->sub.start_display_time / 1000))))
                    {
                        if (sp->uploaded) {
                            int i;
                            for (i = 0; i < sp->sub.num_rects; i++) {
                                AVSubtitleRect *sub_rect = sp->sub.rects[i];
                                uint8_t *pixels;
                                int pitch, j;

                                if (!SDL_LockTexture(is->sub_texture, (SDL_Rect *)sub_rect, (void **)&pixels, &pitch)) {
                                    for (j = 0; j < sub_rect->h; j++, pixels += pitch)
                                        memset(pixels, 0, sub_rect->w << 2);
                                    SDL_UnlockTexture(is->sub_texture);
                                }
                            }
                        }
                        frame_queue_next(&is->subpq);
                    } else {
                        break;
                    }
                }
            }

            frame_queue_next(&is->pictq);
            is->force_refresh = 1;

            if (is->step && !is->paused)
                cpp_stream_toggle_pause(is);
        }
display:
        /* display picture */
        if (!display_disable && is->force_refresh && is->show_mode == VideoState::SHOW_MODE_VIDEO && is->pictq.rindex_shown)
            video_display(is);
    }
    is->force_refresh = 0;
    if (show_status) {
        AVBPrint buf;
        static int64_t last_time;
        int64_t cur_time;
        int aqsize, vqsize, sqsize;
        double av_diff;

        cur_time = av_gettime_relative();
        if (!last_time || (cur_time - last_time) >= 30000) {
            aqsize = 0;
            vqsize = 0;
            sqsize = 0;
            if (is->audio_st)
                aqsize = is->audioq.size;
            if (is->video_st)
                vqsize = rVidPackQ.size();
            if (is->subtitle_st)
                sqsize = is->subtitleq.size;
            av_diff = 0;
            if (is->audio_st && is->video_st) {
                // av_diff = get_clock(&is->audclk) - get_clock(&is->vidclk);
                av_diff = get_clock(&is->audclk) - rVidClk.getClock ();
            }
            else if (is->video_st) {
                // av_diff = cpp_get_master_clock(is) - get_clock(&is->vidclk);
                av_diff = cpp_get_master_clock(is) - rVidClk.getClock();
            }
            else if (is->audio_st)
                av_diff = cpp_get_master_clock(is) - get_clock(&is->audclk);

            av_bprint_init(&buf, 0, AV_BPRINT_SIZE_AUTOMATIC);
            av_bprintf(&buf,
                      "%7.2f %s:%7.3f fd=%4d aq=%5dKB vq=%5dKB sq=%5dB \r",
                      cpp_get_master_clock(is),
                      (is->audio_st && is->video_st) ? "A-V" : (is->video_st ? "M-V" : (is->audio_st ? "M-A" : "   ")),
                      av_diff,
                      is->frame_drops_early + is->frame_drops_late,
                      aqsize / 1024,
                      vqsize / 1024,
                      sqsize);

            if (show_status == 1 && AV_LOG_INFO > av_log_get_level())
                fprintf(stderr, "%s", buf.str);
            else
                av_log(NULL, AV_LOG_INFO, "%s", buf.str);

            fflush(stderr);
            av_bprint_finalize(&buf, NULL);

            last_time = cur_time;
        }
    }
}
}
