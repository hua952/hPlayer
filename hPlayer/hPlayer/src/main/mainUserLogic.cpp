#include "main.h"
#include "mainUserLogic.h"
#include "playerDataRpc.h"

extern "C"
{
#include "ffplayCom.h"
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

