#include "decoTh.h"
#include "ffmpegDecoder.h"

int decoTh::onLoopEnd()
{
    auto pLogic = (ffmpegDecoder*)(userData());
    pLogic->cleanup();
    return 0;
}

