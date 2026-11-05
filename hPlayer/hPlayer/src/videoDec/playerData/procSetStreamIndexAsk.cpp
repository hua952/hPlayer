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

static int sprocSetStreamIndexAsk (videoDecUserLogic& rLogic, videoDec& rServer, const setStreamIndexAsk& rAsk )
{
	gInfo("Rec procSetStreamIndexAsk");
    rLogic.setStreamIndex (rAsk.m_streamIndex);
    return procPacketFunRetType_del;
}
int  videoDec::procSetStreamIndexAsk (const setStreamIndexAsk& rAsk )
{
    return sprocSetStreamIndexAsk(*(dynamic_cast<videoDecUserLogic*>(getIUserLogicWorker ())), *this,rAsk);
}
