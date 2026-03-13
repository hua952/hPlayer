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

static int sprocMainInitOKAsk (decoThUserLogic& rLogic, decoTh& rServer)
{
	gInfo("Rec procMainInitOKAsk");
    rLogic.setState(decoThUserLogic::readState_thisNeetInit);
    return procPacketFunRetType_del;
}

int  decoTh::procMainInitOKAsk ()
{
    return sprocMainInitOKAsk(*(dynamic_cast<decoThUserLogic*>(getIUserLogicWorker ())), *this);
}
