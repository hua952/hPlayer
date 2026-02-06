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

void  decoTh::procVideoDecExitOKNtfAsk ()
{
    auto pLogic = (readPackLogic*)(userData());
    pLogic->clean();
    readPackExitOKNtfAskMsg  msg;
    auto& th = pLogic->getDecoTh ();
    th.sendMsg(msg);
    pLogic->setState (readPackLogic::readState_willExit);
	gInfo("Rec procVideoDecExitOKNtfAsk");
}
