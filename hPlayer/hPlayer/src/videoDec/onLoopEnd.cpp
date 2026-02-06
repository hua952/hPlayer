#include "videoDec.h"
#include "videoDecLogic.h"

int videoDec::onLoopEnd()
{
    auto pLogic = (videoDecLogic*)(userData());
    return pLogic->onLoopEnd();
}

