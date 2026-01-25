#ifndef _globalData_h__
#define _globalData_h__
#include <memory>
#include "comFun.h"
#include <vector>
#include "pSPSCQueue.h"

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
/*
videoFrameData*   frontVideoFrame();
void              popVideoFrame();
bool tryPushVideoFrame(std::unique_ptr<videoFrameData>&&v);
bool tryEmplaceVideoFrame(videoFrameData&& data);
bool mabeVideoFrameFull();
*/
double mainClockSec ();
#endif
