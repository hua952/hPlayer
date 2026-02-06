#include "videoDec.h"
#include "videoDecLogic.h"

int videoDec::onLoopBegin()
{
    static videoDecLogic sDecoder(*this);
    setUserData (&sDecoder);
    return sDecoder.onLoopBegin();
}

