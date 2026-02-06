#include "videoDec.h"
#include "videoDecLogic.h"

int videoDec::onLoopFrame()
{
    auto pLogic = (videoDecLogic*)(userData());
    return pLogic->onLoopFrame();
}

