#include "audioLogic.h"
#include "strFun.h"
#include "audioDec.h"

static inline
int cmp_audio_fmts(enum AVSampleFormat fmt1, int64_t channel_count1,
                   enum AVSampleFormat fmt2, int64_t channel_count2)
{
    /* If channel count == 1, planar and non-planar formats are the same */
    if (channel_count1 == 1 && channel_count2 == 1)
        return av_get_packed_sample_fmt(fmt1) != av_get_packed_sample_fmt(fmt2);
    else
        return channel_count1 != channel_count2 || fmt1 != fmt2;
}

audioLogic:: audioLogic (audioDec& rDec):m_rAudioDec(rDec)
{
}

audioLogic:: ~audioLogic ()
{
}

int  audioLogic:: onLoopBegin()
{
    int  nRet = 0;
    do {
    } while (0);
    return nRet;
}

static int audio_thread(void *arg)
{
    VideoState *is = (VideoState *)arg;
    AVFrame *frame = av_frame_alloc();
    Frame *af;
    int last_serial = -1;
    int reconfigure;
    int got_frame = 0;
    AVRational tb;
    int ret = 0;

    if (!frame)
        return AVERROR(ENOMEM);

    do {
        if ((got_frame = decoder_decode_frame(&is->auddec, frame, NULL)) < 0)
            goto the_end;

        if (got_frame) {
                tb.num = 1; tb.den = frame->sample_rate; // tb = (AVRational){1, frame->sample_rate};

                reconfigure =
                    cmp_audio_fmts(is->audio_filter_src.fmt, is->audio_filter_src.ch_layout.nb_channels,
                                   (AVSampleFormat)frame->format, frame->ch_layout.nb_channels)    ||
                    av_channel_layout_compare(&is->audio_filter_src.ch_layout, &frame->ch_layout) ||
                    is->audio_filter_src.freq           != frame->sample_rate ||
                    is->auddec.pkt_serial               != last_serial;

                if (reconfigure) {
                    char buf1[1024], buf2[1024];
                    av_channel_layout_describe(&is->audio_filter_src.ch_layout, buf1, sizeof(buf1));
                    av_channel_layout_describe(&frame->ch_layout, buf2, sizeof(buf2));
                    av_log(NULL, AV_LOG_DEBUG,
                           "Audio frame changed from rate:%d ch:%d fmt:%s layout:%s serial:%d to rate:%d ch:%d fmt:%s layout:%s serial:%d\n",
                           is->audio_filter_src.freq, is->audio_filter_src.ch_layout.nb_channels, av_get_sample_fmt_name((AVSampleFormat)is->audio_filter_src.fmt), buf1, last_serial,
                           frame->sample_rate, frame->ch_layout.nb_channels, av_get_sample_fmt_name((AVSampleFormat)frame->format), buf2, is->auddec.pkt_serial);

                    is->audio_filter_src.fmt            = (AVSampleFormat)frame->format;
                    ret = av_channel_layout_copy(&is->audio_filter_src.ch_layout, &frame->ch_layout);
                    if (ret < 0)
                        goto the_end;
                    is->audio_filter_src.freq           = frame->sample_rate;
                    last_serial                         = is->auddec.pkt_serial;

                    if ((ret = configure_audio_filters(is, afilters, 1)) < 0)
                        goto the_end;
                }

            if ((ret = av_buffersrc_add_frame(is->in_audio_filter, frame)) < 0)
                goto the_end;

            while ((ret = av_buffersink_get_frame_flags(is->out_audio_filter, frame, 0)) >= 0) {
                AVRational tbTemp;
                FrameData *fd = frame->opaque_ref ? (FrameData*)frame->opaque_ref->data : NULL;
                tb = av_buffersink_get_time_base(is->out_audio_filter);
                if (!(af = frame_queue_peek_writable(&is->sampq)))
                    goto the_end;

                af->pts = (frame->pts == AV_NOPTS_VALUE) ? NAN : frame->pts * av_q2d(tb);
                af->pos = fd ? fd->pkt_pos : -1;
                af->serial = is->auddec.pkt_serial;
                tbTemp.num=frame->nb_samples; tbTemp.den = frame->sample_rate;
                //af->duration = av_q2d((AVRational){frame->nb_samples, frame->sample_rate});
                af->duration = av_q2d(tbTemp);

                av_frame_move_ref(af->frame, frame);
                frame_queue_push(&is->sampq);

                if (is->audioq.serial != is->auddec.pkt_serial)
                    break;
            }
            if (ret == AVERROR_EOF)
                is->auddec.finished = is->auddec.pkt_serial;
        }
    } while (ret >= 0 || ret == AVERROR(EAGAIN) || ret == AVERROR_EOF);
 the_end:
    avfilter_graph_free(&is->agraph);
    av_frame_free(&frame);
    return ret;
}

int  audioLogic:: onLoopFrame()
{
    int  nRet = 0;
    do {
        auto thisState = state();
        if (audioLogicState_readNotInit == thisState ||  audioLogicState_willExit == thisState) [[unlikely]]{
            break;
        }
        if (audioLogicState_thisNeetInit == thisState) [[unlikely]]{
            nRet = initThis ();
            if (procPacketFunRetType_exitNow & nRet || procPacketFunRetType_exitAfterLoop & nRet) [[unlikely]]{
                break;
            }
            setState(audioLogicState_ok);
        }
        VideoState *is = getVideoState ();
        auto& frame = m_frame;
        Frame *af;
        int last_serial = -1;
        int reconfigure;
        int got_frame = 0;
        AVRational tb;
        int ret = 0;
        if ((got_frame = decoder_decode_frame(&is->auddec, frame, NULL)) < 0) {
            clean();
            setState(audioLogicState_willExit);
            break;
            //goto the_end;
        }

        if (got_frame) {
            tb.num = 1; tb.den = frame->sample_rate; // tb = (AVRational){1, frame->sample_rate};

            reconfigure =
                cmp_audio_fmts(is->audio_filter_src.fmt, is->audio_filter_src.ch_layout.nb_channels,
                        (AVSampleFormat)frame->format, frame->ch_layout.nb_channels)    ||
                av_channel_layout_compare(&is->audio_filter_src.ch_layout, &frame->ch_layout) ||
                is->audio_filter_src.freq           != frame->sample_rate ||
                is->auddec.pkt_serial               != last_serial;

            if (reconfigure) {
                char buf1[1024], buf2[1024];
                av_channel_layout_describe(&is->audio_filter_src.ch_layout, buf1, sizeof(buf1));
                av_channel_layout_describe(&frame->ch_layout, buf2, sizeof(buf2));
                av_log(NULL, AV_LOG_DEBUG,
                        "Audio frame changed from rate:%d ch:%d fmt:%s layout:%s serial:%d to rate:%d ch:%d fmt:%s layout:%s serial:%d\n",
                        is->audio_filter_src.freq, is->audio_filter_src.ch_layout.nb_channels, av_get_sample_fmt_name((AVSampleFormat)is->audio_filter_src.fmt), buf1, last_serial,
                        frame->sample_rate, frame->ch_layout.nb_channels, av_get_sample_fmt_name((AVSampleFormat)frame->format), buf2, is->auddec.pkt_serial);

                is->audio_filter_src.fmt            = (AVSampleFormat)frame->format;
                ret = av_channel_layout_copy(&is->audio_filter_src.ch_layout, &frame->ch_layout);
                if (ret < 0) {
                    clean();
                    setState(audioLogicState_willExit);
                    break;
                    // goto the_end;
                }
                is->audio_filter_src.freq           = frame->sample_rate;
                last_serial                         = is->auddec.pkt_serial;

                if ((ret = configure_audio_filters(is, afilters, 1)) < 0) {
                    clean();
                    setState(audioLogicState_willExit);
                    break;
                    // goto the_end;
                }
            }

            if ((ret = av_buffersrc_add_frame(is->in_audio_filter, frame)) < 0) {
                clean();
                setState(audioLogicState_willExit);
                break;
                // goto the_end;
            }

            while ((ret = av_buffersink_get_frame_flags(is->out_audio_filter, frame, 0)) >= 0) {
                AVRational tbTemp;
                FrameData *fd = frame->opaque_ref ? (FrameData*)frame->opaque_ref->data : NULL;
                tb = av_buffersink_get_time_base(is->out_audio_filter);
                if (!(af = frame_queue_peek_writable(&is->sampq))) {
                    clean();
                    setState(audioLogicState_willExit);
                    return nRet;
                    // goto the_end;
                }

                af->pts = (frame->pts == AV_NOPTS_VALUE) ? NAN : frame->pts * av_q2d(tb);
                af->pos = fd ? fd->pkt_pos : -1;
                af->serial = is->auddec.pkt_serial;
                tbTemp.num=frame->nb_samples; tbTemp.den = frame->sample_rate;
                //af->duration = av_q2d((AVRational){frame->nb_samples, frame->sample_rate});
                af->duration = av_q2d(tbTemp);

                av_frame_move_ref(af->frame, frame);
                frame_queue_push(&is->sampq);

                if (is->audioq.serial != is->auddec.pkt_serial)
                    break;
            }
            if (ret == AVERROR_EOF)
                is->auddec.finished = is->auddec.pkt_serial;
        }
        if (!(ret >= 0 || ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)) {
            clean();
            setState(audioLogicState_willExit);
        }
    } while (0);
    return nRet;
}

int  audioLogic:: onLoopEnd()
{
    int  nRet = 0;
    do {
        clean();
    } while (0);
    return nRet;
}

audioLogic::audioLogicState  audioLogic:: state ()
{
    return m_state;
}

void   audioLogic:: setState(audioLogicState st)
{
    m_state = st;
}

int   audioLogic:: initThis()
{
    int   nRet = 0;
    do {
        VideoState *is = getVideoState ();
        auto& frame = m_frame;
        frame = av_frame_alloc();

        if (!frame) {
            // return AVERROR(ENOMEM);
            setState(audioLogicState_willExit);
            break;
        }
    } while (0);
    return nRet;
}

void   audioLogic:: clean()
{
    VideoState *is = getVideoState ();
    avfilter_graph_free(&is->agraph);
    av_frame_free(&m_frame);
}

audioDec&         audioLogic:: getAudioDec()
{
    return m_rAudioDec;
}

