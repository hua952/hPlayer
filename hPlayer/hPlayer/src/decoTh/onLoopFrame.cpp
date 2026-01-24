#include "decoTh.h"
#include "ffmpegDecoder.h"

int decoTh::onLoopFrame()
{
    int nRet = 0;
    do {
        auto pLogic = (ffmpegDecoder*)(userData());
        nRet = pLogic->onLoopFrame();
    } while (0);
    return nRet;
}

