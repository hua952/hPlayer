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

static int sprocPlayURLRet (mainUserLogic& rLogic, main& rServer, const playURLAsk& rAsk, playURLRet& rRet)
{
	gInfo("Rec procPlayURLRet");
    return procPacketFunRetType_del;
}
int  main::procPlayURLRet (const playURLAsk& rAsk, playURLRet& rRet)
{
    return sprocPlayURLRet(*(dynamic_cast<mainUserLogic*>(getIUserLogicWorker ())), *this,rAsk, rRet);
}
