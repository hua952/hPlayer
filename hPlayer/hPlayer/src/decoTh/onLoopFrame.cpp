#include "decoTh.h"
#include "ffmpegDecoder.h"
#include "readPackLogic.h"

int decoTh::onLoopFrame()
{
    int nRet = 0;
    do {
        //auto pLogic = (ffmpegDecoder*)(userData());
        auto pLogic = (readPackLogic*)(userData());
        nRet = pLogic->onLoopFrame();
    } while (0);
    return nRet;
}

