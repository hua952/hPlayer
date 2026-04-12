#include "audioDec.h"
#include "audioDecUserLogic.h"
#include "gLog.h"
#include "cppCom.h"
#include "tSingleton.h"

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
    auto& rGlobal = tSingleton<globalData>::single();
    auto& rDecoder = rGlobal.m_audDec;
    auto& rAudioPackQ = rGlobal.m_audioPackQ;
    auto& rSampQ = rGlobal.m_sampQ;

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

        if (!rSampQ.mabeNeetPush()) {
            break;
        }
        auto& frame = m_frame;
        VideoState *is = getVideoState ();
        cppFrame *af = nullptr;
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
        if ((got_frame = cpp_decoder_decode_frame(rDecoder, frame, NULL)) < 0) {
            // funSendNeetMsg ();
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
                rDecoder.pkt_serial               != last_serial;
// */
            if (reconfigure) {
                char buf1[1024], buf2[1024];
                av_channel_layout_describe(&is->audio_filter_src.ch_layout, buf1, sizeof(buf1));
                av_channel_layout_describe(&frame->ch_layout, buf2, sizeof(buf2));
                av_log(NULL, AV_LOG_DEBUG,
                        "Audio frame changed from rate:%d ch:%d fmt:%s layout:%s serial:%d to rate:%d ch:%d fmt:%s layout:%s serial:%d\n",
                        is->audio_filter_src.freq, is->audio_filter_src.ch_layout.nb_channels, av_get_sample_fmt_name(is->audio_filter_src.fmt), buf1, last_serial,
                        frame->sample_rate, frame->ch_layout.nb_channels, av_get_sample_fmt_name((AVSampleFormat)frame->format), buf2, rDecoder.pkt_serial);

                is->audio_filter_src.fmt            = (AVSampleFormat)frame->format;
                ret = av_channel_layout_copy(&is->audio_filter_src.ch_layout, &frame->ch_layout);
                if (ret < 0) {

                    funSendNeetMsg ();
                    break;
                    // goto the_end;
                }
                is->audio_filter_src.freq           = frame->sample_rate;
                last_serial                         = rDecoder.pkt_serial;

                if ((ret = configure_audio_filters(is, afilters, 1)) < 0) {

                    funSendNeetMsg ();
                    break;
                    // goto the_end;
                }
            }

            if ((ret = av_buffersrc_add_frame(is->in_audio_filter, frame)) < 0) {
                if (ret != AVERROR(EAGAIN)) {
                    funSendNeetMsg ();
                }
                break;
            }
            af = rSampQ.nextWrite();
            static int sA = 0;
            if (!af) {
                sA++;
                gInfo(" af empty "<<sA);
                break;
            }
            do {
                ret = av_buffersink_get_frame_flags(is->out_audio_filter, frame, 0);
                if (ret < 0) {
                    break;
                }
                FrameData *fd = frame->opaque_ref ? (FrameData*)frame->opaque_ref->data : NULL;
                tb = av_buffersink_get_time_base(is->out_audio_filter);
                af->pts = (frame->pts == AV_NOPTS_VALUE) ? NAN : frame->pts * av_q2d(tb);
                af->pos = fd ? fd->pkt_pos : -1;
                af->serial = rDecoder.pkt_serial;

                AVRational tbTemp;
                tbTemp.num=frame->nb_samples; tbTemp.den = frame->sample_rate;
                af->duration = av_q2d(tbTemp);
                av_frame_move_ref(af->frame, frame);

                if (rAudioPackQ.serial() != rDecoder.pkt_serial) {
                    break;
                }

                rSampQ.push();
                af = rSampQ.nextWrite();
                if (!af) {
                    sA++;
                    gInfo("2222 af empty "<<sA);
                }
                if (ret == AVERROR_EOF)
                    rDecoder.finished = rDecoder.pkt_serial;
            } while(af);
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


audioDecUserLogic::bufQue&   audioDecUserLogic:: getBufQue ()
{
    return m_bufQue;
}

