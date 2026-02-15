#include "decoTh.h"
#include "decoThUserLogic.h"
#include "readPackLogic.h"

decoThUserLogic::decoThUserLogic (decoTh& rServer):m_rdecoTh(rServer)
{
}

int decoThUserLogic::onLoopBegin()
{
    int nRet = 0;
    do {
        static readPackLogic sDecoder(getServer());
        getServer().setUserData (&sDecoder);
        return sDecoder.onLoopBegin();
    } while (0);
    return nRet;
}

int decoThUserLogic::onLoopFrame()
{
    int nRet = 0;
    do {
        auto pLogic = (readPackLogic*)(getServer().userData());
        nRet = pLogic->onLoopFrame();
    } while (0);
    return nRet;
}

int decoThUserLogic::onLoopEnd()
{
    int nRet = 0;
    do {
        auto pLogic = (readPackLogic*)(getServer().userData());
        pLogic->onLoopEnd();
    } while (0);
    return nRet;
}
