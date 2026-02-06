#include "tSingleton.h"
#include "msg.h"
#include "gLog.h"
#include "myAssert.h"
#include <memory>
#include "loopHandleS.h"
#include "logicWorker.h"
#include "hPlayerWorkerMgr.h"
#include "mainLogic.h"
#include "playerDataRpc.h"

void  main::procReadPackExitOKNtfAsk ()
{
    auto pLogic = (mainLogic*)(userData());
    pLogic->setState(mainLogic::mainState_willExit);
    ntfOtherLocalServerExit();
    gInfo("Rec procReadPackExitOKNtfAsk");
}
