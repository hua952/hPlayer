#ifndef _globalData_h__
#define _globalData_h__
#include <memory>
#include "comFun.h"
#include <vector>
#include "pSPSCQueue.h"
#include "ffplayCom.h"

struct  videoFrameData
{
    double    m_pts_seconds;
    udword    m_width;
    udword    m_height;
    udword    m_y_linesize;
    udword    m_uv_linesize;
    udword    m_y_planesize;
    udword    m_planNum;
    std::unique_ptr<ubyte[]>  m_plan;
};
typedef pSPSCQueue<videoFrameData> decodeRenderQueue;
decodeRenderQueue&  getDecodeRenderQueue();
double mainClockSec ();
#endif
