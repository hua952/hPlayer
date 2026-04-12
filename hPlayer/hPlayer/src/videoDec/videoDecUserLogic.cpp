#include "videoDec.h"
#include "videoDecUserLogic.h"
#include "playerDataRpc.h"
#include "cppCom.h"
#include "tSingleton.h"

videoDecUserLogic::videoDecUserLogic (videoDec& rServer):m_rvideoDec(rServer)
{
}

int videoDecUserLogic::onLoopBegin()
{
    int nRet = 0;
    do {
    } while (0);
    return nRet;
}






int   videoDecUserLogic:: initThis()
{
    int   nRet = 0;
    do {
        VideoState *is = getVideoState ();
        auto& frame = m_frame;
        AVRational& frame_rate = m_frame_rate;

        frame = av_frame_alloc();
        frame_rate = av_guess_frame_rate(is->ic, is->video_st, NULL);
        if (!frame) {
            nRet = procPacketFunRetType_exitNow;
            break;
        }
    } while (0);
    return nRet;
}

int cpp_get_video_frame(VideoState *is, AVFrame *frame)
{
    int got_picture;

    auto& rGlobal = tSingleton<globalData>::single();
    auto& rDecoder = rGlobal.vidDec;
    auto& rVidPackQ = rGlobal.vidPackQ;
    auto vSize = rVidPackQ.size();

    if ((got_picture = cpp_decoder_decode_frame(rDecoder, frame, NULL)) < 0)
        return -1;

    if (got_picture) {
        double dpts = NAN;

        if (frame->pts != AV_NOPTS_VALUE)
            dpts = av_q2d(is->video_st->time_base) * frame->pts;

        frame->sample_aspect_ratio = av_guess_sample_aspect_ratio(is->ic, is->video_st, frame);

        if (framedrop>0 || (framedrop && get_master_sync_type(is) != AV_SYNC_VIDEO_MASTER)) {
            if (frame->pts != AV_NOPTS_VALUE) {
                double diff = dpts - cpp_get_master_clock(is);
                if (!isnan(diff) && fabs(diff) < AV_NOSYNC_THRESHOLD &&
                    diff - is->frame_last_filter_delay < 0 &&
                    rDecoder.pkt_serial == rVidPackQ.serial() &&
                    vSize) {
                    is->frame_drops_early++;
                    av_frame_unref(frame);
                    got_picture = 0;
                }
            }
        }
    }

    return got_picture;
}

int videoDecUserLogic::onLoopFrame()
{
    int nRet = 0;

    do {
        VideoState *is = getVideoState ();
        auto& frame = m_frame;
        auto& graph = m_graph;
        auto& frame_rate = m_frame_rate;
        auto& filt_out = m_filt_out;
        auto& filt_in = m_filt_in;
        auto& last_w = m_last_w;
        auto& last_h = m_last_h;
        auto& last_format = m_last_format;
        auto& last_serial = m_last_serial;
        auto& last_vfilter_idx = m_last_vfilter_idx;


        auto& rGlobal = tSingleton<globalData>::single();
        auto& rDecoder = rGlobal.vidDec;
        auto& rVidPackQ = rGlobal.vidPackQ;

        double pts;
        double duration;
        int ret;

        auto thisState = state();
        if (videoDecLogicState_readNotInit == thisState || videoDecLogicState_waitExit == thisState || videoDecLogicState_willExit == thisState) [[unlikely]]{
            break;
        }
        if (videoDecLogicState_thisNeetInit == thisState) [[unlikely]]{
            nRet = initThis ();
            if (procPacketFunRetType_exitNow & nRet || procPacketFunRetType_exitAfterLoop & nRet) [[unlikely]]{
                break;
            }
            setState(videoDecLogicState_ok);
        }
        if (!rVidPackQ.mabeNeetPush()) {
            break;
        }
    // for (;;) {
        ret = cpp_get_video_frame(is, frame);
        if (ret < 0) {
            // goto the_end;
            break;
        }
        if (!ret) {
            // continue;
            break;
        }
        if (   last_w != frame->width
            || last_h != frame->height
            || last_format != frame->format
            // || last_serial != is->viddec.pkt_serial
            || last_serial != rDecoder.pkt_serial
            || last_vfilter_idx != is->vfilter_idx) {
            av_log(NULL, AV_LOG_DEBUG,
                   "Video frame changed from size:%dx%d format:%s serial:%d to size:%dx%d format:%s serial:%d\n",
                   last_w, last_h,
                   (const char *)av_x_if_null(av_get_pix_fmt_name(last_format), "none"), last_serial,
                   frame->width, frame->height,
                   // (const char *)av_x_if_null(av_get_pix_fmt_name((enum AVPixelFormat)frame->format), "none"), is->viddec.pkt_serial);
                   (const char *)av_x_if_null(av_get_pix_fmt_name((enum AVPixelFormat)frame->format), "none"), rDecoder.pkt_serial);
            avfilter_graph_free(&graph);
            graph = avfilter_graph_alloc();
            if (!graph) {
                ret = AVERROR(ENOMEM);
                // goto the_end;
                break;
            }
            graph->nb_threads = filter_nbthreads;
            if ((ret = configure_video_filters(graph, is, vfilters_list ? vfilters_list[is->vfilter_idx] : NULL, frame)) < 0) {
                /*
                SDL_Event event;
                event.type = FF_QUIT_EVENT;
                event.user.data1 = is;
                SDL_PushEvent(&event);
                goto the_end;
                */
                NeetExitNtfAskMsg msg;
                getServer().sendMsg(msg);
                setState(videoDecLogicState_waitExit);
                break;
            }
            filt_in  = is->in_video_filter;
            filt_out = is->out_video_filter;
            last_w = frame->width;
            last_h = frame->height;
            last_format = (enum AVPixelFormat)frame->format;
            // last_serial = is->viddec.pkt_serial;
            last_serial = rDecoder.pkt_serial;
            last_vfilter_idx = is->vfilter_idx;
            frame_rate = av_buffersink_get_frame_rate(filt_out);
        }

        ret = av_buffersrc_add_frame(filt_in, frame);
        if (ret < 0) {
            // goto the_end;
            break;
        }

        while (ret >= 0) {
            FrameData *fd;

            is->frame_last_returned_time = av_gettime_relative() / 1000000.0;

            ret = av_buffersink_get_frame_flags(filt_out, frame, 0);
            if (ret < 0) {
                if (ret == AVERROR_EOF)
                    rDecoder.finished = rDecoder.pkt_serial;
                ret = 0;
                break;
            }

            fd = frame->opaque_ref ? (FrameData*)frame->opaque_ref->data : NULL;

            is->frame_last_filter_delay = av_gettime_relative() / 1000000.0 - is->frame_last_returned_time;
            if (fabs(is->frame_last_filter_delay) > AV_NOSYNC_THRESHOLD / 10.0)
                is->frame_last_filter_delay = 0;
            auto tb = av_buffersink_get_time_base(filt_out);
            AVRational tbTemp;
            tbTemp.num = frame_rate.den; tbTemp.den = frame_rate.num;
            duration = (frame_rate.num && frame_rate.den ? av_q2d(tbTemp) : 0);
            // duration = (frame_rate.num && frame_rate.den ? av_q2d((AVRational){frame_rate.den, frame_rate.num}) : 0);
            pts = (frame->pts == AV_NOPTS_VALUE) ? NAN : frame->pts * av_q2d(tb);

            // ret = queue_picture(is, frame, pts, duration, fd ? fd->pkt_pos : -1, is->viddec.pkt_serial);
            ret = cpp_queue_picture(is, frame, pts, duration, fd ? fd->pkt_pos : -1, rDecoder.pkt_serial);
            av_frame_unref(frame);
            // if (is->videoq.serial != is->viddec.pkt_serial) {
            if (rVidPackQ.serial() != rDecoder.pkt_serial) {
                break;
            }
        }

        if (ret < 0) {
            // goto the_end;
            break;
        }
    // }
        /*
 the_end:
    avfilter_graph_free(&graph);
    av_frame_free(&frame);
    return 0;
    */
    } while (0);
    
    return nRet;
}

int videoDecUserLogic::onLoopEnd()
{
    int nRet = 0;
    do {
        clean();
    } while (0);
    return nRet;
}

void   videoDecUserLogic:: clean()
{
    do {
        avfilter_graph_free(&m_graph);
        av_frame_free(&m_frame);
    } while (0);
}

videoDecUserLogic::videoDecLogicState   videoDecUserLogic:: state ()
{
    return m_state;
}

void videoDecUserLogic:: setState(videoDecLogicState  st)
{
    m_state = st;
}

/*
static void video_decoder_abort(Decoder *d, FrameQueue *fq)
{
    packet_queue_abort(d->queue);
    frame_queue_signal(fq);
    // SDL_WaitThread(d->decoder_tid, NULL);
    // d->decoder_tid = NULL;
    packet_queue_flush(d->queue);
}
*/
extern "C"
{
    void cleanVideoDec()
    {
        auto& rGlobal = tSingleton<globalData>::single();
        auto& rDecoder = rGlobal.vidDec;
        av_packet_free(&rDecoder.pkt);
        avcodec_free_context(&rDecoder.avctx);
    }
}
