#include "videoDec.h"
#include "videoDecUserLogic.h"
#include "playerDataRpc.h"
#include "cppCom.h"
#include "tSingleton.h"
#include <memory>

static enum AVColorSpace sdl_supported_color_spaces[] = {
    AVCOL_SPC_BT709,
    AVCOL_SPC_BT470BG,
    AVCOL_SPC_SMPTE170M,
};


int videoDecUserLogic::configure_video_filters(AVFilterGraph *graph, VideoState *is, const char *vfilters, AVFrame *frame)
{
    auto& rGlobal = tSingleton<globalData>::single();
    int enterNum = 0;
    auto entryMap = getFormatEntry(&enterNum);
    auto pix_fmts_ptr = std::make_unique<enum AVPixelFormat []>(enterNum);
    auto pix_fmts = pix_fmts_ptr.get();
    //enum AVPixelFormat pix_fmts[FF_ARRAY_ELEMS(sdl_texture_format_map)];
    char sws_flags_str[512] = "";
    int ret;
    AVFilterContext *filt_src = NULL, *filt_out = NULL, *last_filter = NULL;
    AVCodecParameters *codecpar = rGlobal.videoSt()->codecpar;
    AVRational fr = av_guess_frame_rate(is->ic, rGlobal.videoSt(), NULL);
    const AVDictionaryEntry *e = NULL;
    int nb_pix_fmts = 0;
    int i, j;
    AVBufferSrcParameters *par = av_buffersrc_parameters_alloc();

    if (!par)
        return AVERROR(ENOMEM);

    for (i = 0; i < renderer_info.num_texture_formats; i++) {
        for (j = 0; j < enterNum; j++) {
            if (renderer_info.texture_formats[i] == sdl_texture_format_map[j].texture_fmt) {
                pix_fmts[nb_pix_fmts++] = sdl_texture_format_map[j].format;
                break;
            }
        }
    }

    while ((e = av_dict_iterate(sws_dict, e))) {
        if (!strcmp(e->key, "sws_flags")) {
            av_strlcatf(sws_flags_str, sizeof(sws_flags_str), "%s=%s:", "flags", e->value);
        } else
            av_strlcatf(sws_flags_str, sizeof(sws_flags_str), "%s=%s:", e->key, e->value);
    }
    if (strlen(sws_flags_str))
        sws_flags_str[strlen(sws_flags_str)-1] = '\0';

    graph->scale_sws_opts = av_strdup(sws_flags_str);


    filt_src = avfilter_graph_alloc_filter(graph, avfilter_get_by_name("buffer"),
                                           "ffplay_buffer");
    if (!filt_src) {
        ret = AVERROR(ENOMEM);
        goto fail;
    }

    par->format              = frame->format;
    par->time_base           = rGlobal.videoSt()->time_base;
    par->width               = frame->width;
    par->height              = frame->height;
    par->sample_aspect_ratio = codecpar->sample_aspect_ratio;
    par->color_space         = frame->colorspace;
    par->color_range         = frame->color_range;
    par->frame_rate          = fr;
    par->hw_frames_ctx = frame->hw_frames_ctx;
    ret = av_buffersrc_parameters_set(filt_src, par);
    if (ret < 0)
        goto fail;

    ret = avfilter_init_dict(filt_src, NULL);
    if (ret < 0)
        goto fail;

    filt_out = avfilter_graph_alloc_filter(graph, avfilter_get_by_name("buffersink"),
                                           "ffplay_buffersink");
    if (!filt_out) {
        ret = AVERROR(ENOMEM);
        goto fail;
    }

    if ((ret = av_opt_set_array(filt_out, "pixel_formats", AV_OPT_SEARCH_CHILDREN,
                                0, nb_pix_fmts, AV_OPT_TYPE_PIXEL_FMT, pix_fmts)) < 0)
        goto fail;
    if (!vk_renderer &&
        (ret = av_opt_set_array(filt_out, "colorspaces", AV_OPT_SEARCH_CHILDREN,
                                0, FF_ARRAY_ELEMS(sdl_supported_color_spaces),
                                AV_OPT_TYPE_INT, sdl_supported_color_spaces)) < 0)
        goto fail;

    ret = avfilter_init_dict(filt_out, NULL);
    if (ret < 0)
        goto fail;

    last_filter = filt_out;

/* Note: this macro adds a filter before the lastly added filter, so the
 * processing order of the filters is in reverse */
#define INSERT_FILT(name, arg) do {                                          \
    AVFilterContext *filt_ctx;                                               \
                                                                             \
    ret = avfilter_graph_create_filter(&filt_ctx,                            \
                                       avfilter_get_by_name(name),           \
                                       "ffplay_" name, arg, NULL, graph);    \
    if (ret < 0)                                                             \
        goto fail;                                                           \
                                                                             \
    ret = avfilter_link(filt_ctx, 0, last_filter, 0);                        \
    if (ret < 0)                                                             \
        goto fail;                                                           \
                                                                             \
    last_filter = filt_ctx;                                                  \
} while (0)

    if (autorotate) {
        double theta = 0.0;
        int32_t *displaymatrix = NULL;
        AVFrameSideData *sd = av_frame_get_side_data(frame, AV_FRAME_DATA_DISPLAYMATRIX);
        if (sd)
            displaymatrix = (int32_t *)sd->data;
        if (!displaymatrix) {
            const AVPacketSideData *psd = av_packet_side_data_get(rGlobal.videoSt()->codecpar->coded_side_data,
                                                                  rGlobal.videoSt()->codecpar->nb_coded_side_data,
                                                                  AV_PKT_DATA_DISPLAYMATRIX);
            if (psd)
                displaymatrix = (int32_t *)psd->data;
        }
        theta = get_rotation(displaymatrix);

        if (fabs(theta - 90) < 1.0) {
            INSERT_FILT("transpose", displaymatrix[3] > 0 ? "cclock_flip" : "clock");
        } else if (fabs(theta - 180) < 1.0) {
            if (displaymatrix[0] < 0)
                INSERT_FILT("hflip", NULL);
            if (displaymatrix[4] < 0)
                INSERT_FILT("vflip", NULL);
        } else if (fabs(theta - 270) < 1.0) {
            INSERT_FILT("transpose", displaymatrix[3] < 0 ? "clock_flip" : "cclock");
        } else if (fabs(theta) > 1.0) {
            char rotate_buf[64];
            snprintf(rotate_buf, sizeof(rotate_buf), "%f*PI/180", theta);
            INSERT_FILT("rotate", rotate_buf);
        } else {
            if (displaymatrix && displaymatrix[4] < 0)
                INSERT_FILT("vflip", NULL);
        }
    }

    if ((ret = configure_filtergraph(graph, vfilters, filt_src, last_filter)) < 0)
        goto fail;

    // is->in_video_filter  = filt_src;
    // is->out_video_filter = filt_out;
    setIn_video_filter (filt_src);
    setOut_video_filter (filt_out);
fail:
    av_freep(&par);
    return ret;
}


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
        auto& rGlobal = tSingleton<globalData>::single();
        auto& frame = m_frame;
        AVRational& frame_rate = m_frame_rate;

        frame = av_frame_alloc();
        frame_rate = av_guess_frame_rate(is->ic, rGlobal.videoSt(), NULL);
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
            dpts = av_q2d(rGlobal.videoSt()->time_base) * frame->pts;

        frame->sample_aspect_ratio = av_guess_sample_aspect_ratio(is->ic, rGlobal.videoSt(), frame);

        if (framedrop>0 || (framedrop && cpp_get_master_sync_type(is) != AV_SYNC_VIDEO_MASTER)) {
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
        auto& rGlobal = tSingleton<globalData>::single();
        if (rGlobal.abort())  [[unlikely]]{
            nRet = procPacketFunRetType_exitNow;
            break;
        }
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
                /*
                NeetExitNtfAskMsg msg;
                getServer().sendMsg(msg);
                setState(videoDecLogicState_waitExit);
                */
                rGlobal.setAbort(true);
                break;
            }
            filt_in  = in_video_filter (); //is->in_video_filter;
            filt_out = out_video_filter (); //is->out_video_filter;
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


logicWorker& videoDecUserLogic:: worker()
{
    return getServer();
}

AVFilterContext*  videoDecUserLogic:: in_video_filter ()
{
    return m_in_video_filter;
}

void  videoDecUserLogic:: setIn_video_filter (AVFilterContext* v)
{
    m_in_video_filter = v;
}

AVFilterContext*  videoDecUserLogic:: out_video_filter ()
{
    return m_out_video_filter;
}

void  videoDecUserLogic:: setOut_video_filter (AVFilterContext* v)
{
    m_out_video_filter = v;
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
    
}
