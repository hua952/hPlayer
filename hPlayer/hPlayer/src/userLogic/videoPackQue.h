#ifndef _videoPackQue_h__
#define _videoPackQue_h__
#include <atomic>
#include <memory>
#include <libavcodec/avcodec.h>
#include "packQue.h"

class videoPackQue:public packQue
{
public:
    explicit videoPackQue ();
};
#endif
