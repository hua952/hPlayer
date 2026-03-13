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

static int sprocAudioDecExitOKNtfAsk (decoThUserLogic& rLogic, decoTh& rServer)
{
    rLogic.clean();
    readPackExitOKNtfAskMsg  msg;
    rServer.sendMsg(msg);
    rLogic.setState (decoThUserLogic::readState_willExit);
	gInfo("Rec procAudioDecExitOKNtfAsk");
    return procPacketFunRetType_del;
}
int  decoTh::procAudioDecExitOKNtfAsk ()
{
    return sprocAudioDecExitOKNtfAsk(*(dynamic_cast<decoThUserLogic*>(getIUserLogicWorker ())), *this);
}
