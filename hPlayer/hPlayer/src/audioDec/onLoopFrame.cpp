#include "audioDec.h"
#include "audioLogic.h"

int audioDec::onLoopFrame()
{
    auto pLogic = (audioLogic*)(userData());
    return pLogic->onLoopFrame();
}

