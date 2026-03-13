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

static int sprocVideoDecExitOKNtfAsk (decoThUserLogic& rLogic, decoTh& rServer)
{
    // readPackExitOKNtfAskMsg  msg;
    // rServer.sendMsg(msg);

    audioDecExitNtfAskMsg ask;
    rServer.sendMsg(ask);
    rLogic.sendEmptyAudioPack();
	gInfo("Rec procVideoDecExitOKNtfAsk");
    return procPacketFunRetType_del;
}
int  decoTh::procVideoDecExitOKNtfAsk ()
{
    return sprocVideoDecExitOKNtfAsk(*(dynamic_cast<decoThUserLogic*>(getIUserLogicWorker ())), *this);
}
