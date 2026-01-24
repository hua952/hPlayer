#include "globalData.h"
#include "strFun.h"
#include <cstring>
#include <string>
#include "myAssert.h"
#include "logicWorker.h"
#include "logicWorkerMgr.h"
#include "tSingleton.h"
#include "gLog.h"
#include "hPlayerWorkerMgr.h"
#include <SDL.h>
#include "tSingleton.h"
#include "hPlayerConfig.h"
#include "ffmpegDecoder.h"
#include "SPSCQueue.h"


double mainClockSec ()
{
	auto &rMgr = tSingleton<hPlayerWorkerMgr>::single();
    auto pLogic = (ffmpegDecoder*)( rMgr.m_decoTh.first[0].userData());
    return pLogic->audioClockSec ();
}

static rigtorp::SPSCQueue<std::unique_ptr<videoFrameData>> sVideoFrameQue(128);

videoFrameData*     frontVideoFrame()
{
    videoFrameData*    nRet = nullptr;
    do {
        auto pF = sVideoFrameQue.front();
        if (pF) {
            nRet = pF->get();
        }
    } while (0);
    return nRet;
}

void     popVideoFrame()
{
    sVideoFrameQue.pop();
}

bool tryPushVideoFrame(std::unique_ptr<videoFrameData>&&v)
{
    return sVideoFrameQue.try_push(std::move(v));
}

bool tryEmplaceVideoFrame(videoFrameData&& data)
{
    return sVideoFrameQue.try_emplace(
        std::make_unique<videoFrameData>(std::move(data))
    );
}

/* 可能队列满了，非准确性判断 */
bool mabeVideoFrameFull()
{
    return (sVideoFrameQue.size() >= sVideoFrameQue.capacity());
}
