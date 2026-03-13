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

static int sprocNeetExitNtfAsk (mainUserLogic& rLogic, main& rServer)
{
    readPackExitNtfAskMsg msg;
    rServer.sendMsg(msg);
	gInfo("Rec procNeetExitNtfAsk");
    return procPacketFunRetType_del;
}
int  main::procNeetExitNtfAsk ()
{
    return sprocNeetExitNtfAsk(*(dynamic_cast<mainUserLogic*>(getIUserLogicWorker ())), *this);
}
