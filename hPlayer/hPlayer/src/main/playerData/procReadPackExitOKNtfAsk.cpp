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

static int sprocReadPackExitOKNtfAsk (mainUserLogic& rLogic, main& rServer)
{
	gInfo("Rec procReadPackExitOKNtfAsk");

    rLogic.setState(mainUserLogic::mainState_willExit);
    rServer.ntfOtherLocalServerExit();
    return procPacketFunRetType_del;
}
int  main::procReadPackExitOKNtfAsk ()
{
    return sprocReadPackExitOKNtfAsk(*(dynamic_cast<mainUserLogic*>(getIUserLogicWorker ())), *this);
}
