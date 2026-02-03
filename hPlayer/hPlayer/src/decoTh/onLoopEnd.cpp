#include "decoTh.h"
#include "ffmpegDecoder.h"
#include "readPackLogic.h"

int decoTh::onLoopEnd()
{
    auto pLogic = (readPackLogic*)(userData());
    pLogic->onLoopEnd();
    return 0;
}

