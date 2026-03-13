#include "subtitleqDec.h"
#include "subtitleqDecUserLogic.h"

subtitleqDecUserLogic::subtitleqDecUserLogic (subtitleqDec& rServer):m_rsubtitleqDec(rServer)
{
}

int subtitleqDecUserLogic::onLoopBegin()
{
    int nRet = 0;
    do {
    } while (0);
    return nRet;
}

int   subtitleqDecUserLogic:: initThis()
{
    int   nRet = 0;
    do {
    } while (0);
    return nRet;
}

int subtitleqDecUserLogic::onLoopFrame()
{
    int nRet = 0;
    do {
        auto thisState = state();
        if (subtitleqLogicState_readNotInit == thisState ||  subtitleqLogicState_willExit == thisState) [[unlikely]]{
            break;
        }
        if (subtitleqLogicState_thisNeetInit == thisState) [[unlikely]]{
            nRet = initThis ();
            if (procPacketFunRetType_exitNow & nRet || procPacketFunRetType_exitAfterLoop & nRet) [[unlikely]]{
                break;
            }
            setState(subtitleqLogicState_ok);
        }

        VideoState *is = getVideoState ();
    Frame *sp;
    int got_subtitle;
    double pts;
    auto funSendNeetMsg = [this]() {
            NeetExitNtfAskMsg msg;
            getServer().sendMsg(msg);
            setState(subtitleqLogicState_willExit);
        };
    // for (;;) {
        if (!(sp = frame_queue_peek_writable(&is->subpq))) {
            funSendNeetMsg ();
            break;
        }

        if ((got_subtitle = decoder_decode_frame(&is->subdec, NULL, &sp->sub)) < 0) {
            funSendNeetMsg ();
            break;
        }
        pts = 0;

        if (got_subtitle && sp->sub.format == 0) {
            if (sp->sub.pts != AV_NOPTS_VALUE)
                pts = sp->sub.pts / (double)AV_TIME_BASE;
            sp->pts = pts;
            sp->serial = is->subdec.pkt_serial;
            sp->width = is->subdec.avctx->width;
            sp->height = is->subdec.avctx->height;
            sp->uploaded = 0;

            /* now we can update the picture count */
            frame_queue_push(&is->subpq);
        } else if (got_subtitle) {
            avsubtitle_free(&sp->sub);
        }
    //}
    } while (0);
    return nRet;
}

int subtitleqDecUserLogic::onLoopEnd()
{
    int nRet = 0;
    do {
        
    } while (0);
    return nRet;
}

subtitleqDecUserLogic::subtitleqLogicState   subtitleqDecUserLogic:: state ()
{
    return m_state;
}

void subtitleqDecUserLogic:: setState (subtitleqLogicState st)
{
    m_state = st;
}

void   subtitleqDecUserLogic:: clean()
{
    do {
    } while (0);
}

