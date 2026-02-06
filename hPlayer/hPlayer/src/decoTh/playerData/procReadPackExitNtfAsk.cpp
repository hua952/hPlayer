#include "tSingleton.h"
#include "msg.h"
#include "gLog.h"
#include "myAssert.h"
#include <memory>
#include "loopHandleS.h"
#include "logicWorker.h"
#include "hPlayerWorkerMgr.h"
#include "readPackLogic.h"
#include "playerDataRpc.h"

void  decoTh::procReadPackExitNtfAsk ()
{
    auto pLogic = (readPackLogic*)(userData());
    pLogic->sendExitNtfToSub();
	gInfo("Rec procReadPackExitNtfAsk");
}
