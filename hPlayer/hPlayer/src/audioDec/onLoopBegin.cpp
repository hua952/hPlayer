#include "audioDec.h"
#include "audioLogic.h"

int audioDec::onLoopBegin()
{
    static audioLogic sDecoder(*this);
    setUserData (&sDecoder);
    return sDecoder.onLoopBegin();
}

