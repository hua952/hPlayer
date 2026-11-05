#include "tSingleton.h"
#include "msg.h"
#include "gLog.h"
#include "myAssert.h"
#include <memory>
#include "loopHandleS.h"
#include "logicWorker.h"
#include "audioDecUserLogic.h"

#include "hPlayerWorkerMgr.h"

#include "playerDataRpc.h"

static int sprocSetStreamIndexAsk (audioDecUserLogic& rLogic, audioDec& rServer, const setStreamIndexAsk& rAsk )
{
	gInfo("Rec procSetStreamIndexAsk");
    rLogic.setStreamIndex (rAsk.m_streamIndex);
    return procPacketFunRetType_del;
}
int  audioDec::procSetStreamIndexAsk (const setStreamIndexAsk& rAsk )
{
    return sprocSetStreamIndexAsk(*(dynamic_cast<audioDecUserLogic*>(getIUserLogicWorker ())), *this,rAsk);
}
