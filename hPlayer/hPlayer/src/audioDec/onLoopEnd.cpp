#include "audioDec.h"
#include "audioLogic.h"

int audioDec::onLoopEnd()
{
    auto pLogic = (audioLogic*)(userData());
    return pLogic->onLoopEnd();
}

