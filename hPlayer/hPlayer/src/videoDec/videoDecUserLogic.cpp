#include "videoDec.h"
#include "videoDecUserLogic.h"
#include "videoDecLogic.h"

videoDecUserLogic::videoDecUserLogic (videoDec& rServer):m_rvideoDec(rServer)
{
}

int videoDecUserLogic::onLoopBegin()
{
    int nRet = 0;
    do {
        static videoDecLogic sLogic(getServer());
        getServer().setUserData (&sLogic);
        nRet = sLogic.onLoopBegin();
    } while (0);
    return nRet;
}

int videoDecUserLogic::onLoopFrame()
{
    int nRet = 0;
    do {
        auto pLogic = (videoDecLogic*)(getServer().userData());
        nRet = pLogic->onLoopFrame();
    } while (0);
    return nRet;
}

int videoDecUserLogic::onLoopEnd()
{
    int nRet = 0;
    do {
        auto pLogic = (videoDecLogic*)(getServer().userData());
        nRet = pLogic->onLoopEnd();
    } while (0);
    return nRet;
}
