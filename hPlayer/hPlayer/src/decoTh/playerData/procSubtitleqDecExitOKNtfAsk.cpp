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

static int sprocSubtitleqDecExitOKNtfAsk (decoThUserLogic& rLogic, decoTh& rServer)
{

    videoDecExitNtfAskMsg msg;
    rServer.sendMsg(msg);
	gInfo("Rec procSubtitleqDecExitOKNtfAsk");
    return procPacketFunRetType_del;
}
int  decoTh::procSubtitleqDecExitOKNtfAsk ()
{
    return sprocSubtitleqDecExitOKNtfAsk(*(dynamic_cast<decoThUserLogic*>(getIUserLogicWorker ())), *this);
}
