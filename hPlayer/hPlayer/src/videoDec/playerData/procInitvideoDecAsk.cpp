#include "tSingleton.h"
#include "msg.h"
#include "gLog.h"
#include "myAssert.h"
#include <memory>
#include "loopHandleS.h"
#include "logicWorker.h"
#include "videoDecUserLogic.h"

#include "hPlayerWorkerMgr.h"

#include "playerDataRpc.h"

static int sprocInitvideoDecAsk (videoDecUserLogic& rLogic, videoDec& rServer, const initvideoDecAsk& rAsk )
{
	gInfo("Rec procInitvideoDecAsk");
    int nRet = procPacketFunRetType_del;
    // rLogic.setState(videoDecUserLogic::videoDecLogicState_thisNeetInit);
    auto ret = rLogic.initVideo(rAsk.m_streamIndex);
    if(ret < 0) [[unlikely]]{
        gError("initVideo error ret = "<<ret<<" streamIndex = "<<rAsk.m_streamIndex);
        nRet = procPacketFunRetType_exitNow;
    }
    return nRet;
}
int  videoDec::procInitvideoDecAsk (const initvideoDecAsk& rAsk )
{
    return sprocInitvideoDecAsk(*(dynamic_cast<videoDecUserLogic*>(getIUserLogicWorker ())), *this,rAsk);
}
