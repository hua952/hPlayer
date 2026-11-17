#include "tSingleton.h"
#include "msg.h"
#include "gLog.h"
#include "myAssert.h"
#include <memory>
#include "loopHandleS.h"
#include "logicWorker.h"
#include "mainUserLogic.h"

#include "hPlayerWorkerMgr.h"

#include "playerDataRpc.h"

static int sprocDecUpdateToMainAsk (mainUserLogic& rLogic, main& rServer, const decUpdateToMainAsk& rAsk )
{
	gInfo("Rec procDecUpdateToMainAsk");
    rLogic.setMax_frame_duration (rAsk.m_max_frame_duration);
    return procPacketFunRetType_del;
}
int  main::procDecUpdateToMainAsk (const decUpdateToMainAsk& rAsk )
{
    return sprocDecUpdateToMainAsk(*(dynamic_cast<mainUserLogic*>(getIUserLogicWorker ())), *this,rAsk);
}
