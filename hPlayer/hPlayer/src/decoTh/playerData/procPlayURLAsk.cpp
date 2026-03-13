#include "tSingleton.h"
#include "msg.h"
#include "gLog.h"
#include "myAssert.h"
#include <memory>
#include "loopHandleS.h"
#include "logicWorker.h"
#include "decoThUserLogic.h"

#include "hPlayerWorkerMgr.h"

#include "playerDataRpc.h"

static int sprocPlayURLAsk (decoThUserLogic& rLogic, decoTh& rServer, const playURLAsk& rAsk , playURLRet& rRet)
{
	gInfo("Rec procPlayURLAsk");
    return procPacketFunRetType_del;
}
int  decoTh::procPlayURLAsk (const playURLAsk& rAsk , playURLRet& rRet)
{
    return sprocPlayURLAsk(*(dynamic_cast<decoThUserLogic*>(getIUserLogicWorker ())), *this,rAsk, rRet);
}
