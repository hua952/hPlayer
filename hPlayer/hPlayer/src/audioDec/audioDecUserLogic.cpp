#include "audioDec.h"
#include "audioDecUserLogic.h"
#include "gLog.h"

audioDecUserLogic::audioDecUserLogic (audioDec& rServer):m_raudioDec(rServer)
{
}

int audioDecUserLogic::onLoopBegin()
{
    int nRet = 0;
    do {
    } while (0);
    return nRet;
}

int   audioDecUserLogic:: initThis()
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


int audioDecUserLogic::onLoopFrame()
{
    int nRet = 0;
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
        auto& frame = m_frame;
        VideoState *is = getVideoState ();
        Frame *af;
        int last_serial = -1;
        int reconfigure;
        int got_frame = 0;
        AVRational tb;
        int ret = 0;

        auto funSendNeetMsg = [this]() {
            NeetExitNtfAskMsg msg;
            getServer().sendMsg(msg);
            setState(audioLogicState_willExit);
        };
        // do {
        if ((got_frame = decoder_decode_frame(&is->auddec, frame, NULL)) < 0) {
            funSendNeetMsg ();
            break;
            // goto the_end;
        }

        if (got_frame) {
            tb.num = 1; tb.den = frame->sample_rate; //tb = (AVRational){1, frame->sample_rate};
// /*
            reconfigure =
                cmp_audio_fmts(is->audio_filter_src.fmt, is->audio_filter_src.ch_layout.nb_channels,
                        (AVSampleFormat)frame->format, frame->ch_layout.nb_channels)    ||
                av_channel_layout_compare(&is->audio_filter_src.ch_layout, &frame->ch_layout) ||
                is->audio_filter_src.freq           != frame->sample_rate ||
                is->auddec.pkt_serial               != last_serial;
// */
            if (reconfigure) {
                char buf1[1024], buf2[1024];
                av_channel_layout_describe(&is->audio_filter_src.ch_layout, buf1, sizeof(buf1));
                av_channel_layout_describe(&frame->ch_layout, buf2, sizeof(buf2));
                av_log(NULL, AV_LOG_DEBUG,
                        "Audio frame changed from rate:%d ch:%d fmt:%s layout:%s serial:%d to rate:%d ch:%d fmt:%s layout:%s serial:%d\n",
                        is->audio_filter_src.freq, is->audio_filter_src.ch_layout.nb_channels, av_get_sample_fmt_name(is->audio_filter_src.fmt), buf1, last_serial,
                        frame->sample_rate, frame->ch_layout.nb_channels, av_get_sample_fmt_name((AVSampleFormat)frame->format), buf2, is->auddec.pkt_serial);

                is->audio_filter_src.fmt            = (AVSampleFormat)frame->format;
                ret = av_channel_layout_copy(&is->audio_filter_src.ch_layout, &frame->ch_layout);
                if (ret < 0) {

                    funSendNeetMsg ();
                    break;
                    // goto the_end;
                }
                is->audio_filter_src.freq           = frame->sample_rate;
                last_serial                         = is->auddec.pkt_serial;

                if ((ret = configure_audio_filters(is, afilters, 1)) < 0) {

                    funSendNeetMsg ();
                    break;
                    // goto the_end;
                }
            }

            if ((ret = av_buffersrc_add_frame(is->in_audio_filter, frame)) < 0) {

                funSendNeetMsg ();
                break;
                // goto the_end;
            }

            while ((ret = av_buffersink_get_frame_flags(is->out_audio_filter, frame, 0)) >= 0) {
                FrameData *fd = frame->opaque_ref ? (FrameData*)frame->opaque_ref->data : NULL;
                tb = av_buffersink_get_time_base(is->out_audio_filter);
                if (!(af = frame_queue_peek_writable(&is->sampq))) {

                    funSendNeetMsg ();
                    return nRet;
                    // goto the_end;
                }

                af->pts = (frame->pts == AV_NOPTS_VALUE) ? NAN : frame->pts * av_q2d(tb);
                af->pos = fd ? fd->pkt_pos : -1;
                af->serial = is->auddec.pkt_serial;

                AVRational tbTemp;
                tbTemp.num=frame->nb_samples; tbTemp.den = frame->sample_rate;
                af->duration = av_q2d(tbTemp);
                // af->duration = av_q2d((AVRational){frame->nb_samples, frame->sample_rate});

                av_frame_move_ref(af->frame, frame);
                frame_queue_push(&is->sampq);

                if (is->audioq.serial != is->auddec.pkt_serial) {
                    break;
                }
            }
            if (ret == AVERROR_EOF)
                is->auddec.finished = is->auddec.pkt_serial;
        }
        // } while (ret >= 0 || ret == AVERROR(EAGAIN) || ret == AVERROR_EOF);
        if (!(ret >= 0 || ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)) {
            funSendNeetMsg ();
            break;
        }
    } while (0);
    return nRet;
}

int audioDecUserLogic::onLoopEnd()
{
    int nRet = 0;
    do {
        clean();
	    gInfo("audio will end");
    } while (0);
    return nRet;
}

void   audioDecUserLogic:: clean()
{
    do {

    VideoState *is = getVideoState ();
    avfilter_graph_free(&is->agraph);
    av_frame_free(&m_frame);

    } while (0);
}

audioDecUserLogic::audioLogicState  audioDecUserLogic:: state ()
{
    return m_state;
}

void   audioDecUserLogic:: setState(audioLogicState st)
{
    m_state = st;
}


