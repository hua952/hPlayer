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

static int sprocSeekPosRet (mainUserLogic& rLogic, main& rServer, const seekPosAsk& rAsk, seekPosRet& rRet)
{
	gInfo("Rec procSeekPosRet");
    return procPacketFunRetType_del;
}
int  main::procSeekPosRet (const seekPosAsk& rAsk, seekPosRet& rRet)
{
    return sprocSeekPosRet(*(dynamic_cast<mainUserLogic*>(getIUserLogicWorker ())), *this,rAsk, rRet);
}
