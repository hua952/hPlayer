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

static int sprocSeekPosAsk (decoThUserLogic& rLogic, decoTh& rServer, const seekPosAsk& rAsk , seekPosRet& rRet)
{
	gInfo("Rec procSeekPosAsk");
    return procPacketFunRetType_del;
}
int  decoTh::procSeekPosAsk (const seekPosAsk& rAsk , seekPosRet& rRet)
{
    return sprocSeekPosAsk(*(dynamic_cast<decoThUserLogic*>(getIUserLogicWorker ())), *this,rAsk, rRet);
}
