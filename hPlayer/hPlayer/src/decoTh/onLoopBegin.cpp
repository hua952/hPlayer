#include "decoTh.h"
#include "ffmpegDecoder.h"

int decoTh::onLoopBegin()
{
    static ffmpegDecoder sDecoder(*this);
    setUserData (&sDecoder);
    return sDecoder.init();
}

