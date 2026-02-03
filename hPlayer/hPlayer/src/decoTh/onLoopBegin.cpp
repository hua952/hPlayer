#include "decoTh.h"
#include "ffmpegDecoder.h"
#include "readPackLogic.h"

int decoTh::onLoopBegin()
{
    // static ffmpegDecoder sDecoder(*this);
    // setUserData (&sDecoder);
    static readPackLogic sDecoder(*this);
    setUserData (&sDecoder);
    return sDecoder.onLoopBegin();
}

