#include "main.h"
#include "mainUserLogic.h"
#include "mainLogic.h"

mainUserLogic::mainUserLogic (main& rServer):m_rmain(rServer)
{
}

int mainUserLogic::onLoopBegin()
{
    int nRet = 0;
    do {
        static mainLogic sLogic(getServer());
        getServer().setUserData (&sLogic);
        nRet = sLogic.onLoopBegin();
    } while (0);
    return nRet;
}

int mainUserLogic::onLoopFrame()
{
    int nRet = 0;
    do {
        auto pLogic = (mainLogic*)(getServer().userData());
        nRet = pLogic->onLoopFrame();
        if (procPacketFunRetType_exitAfterLoop & nRet) {
            getServer().ntfOtherLocalServerExit ();
        }
    } while (0);
    return nRet;
}

int mainUserLogic::onLoopEnd()
{
    int nRet = 0;
    do {
        auto pLogic = (mainLogic*)(getServer().userData());
        nRet = pLogic->onLoopEnd();
    } while (0);
    return nRet;
}
