#include "tSingleton.h"
#include "msg.h"
#include "gLog.h"
#include "myAssert.h"
#include <memory>
#include "loopHandleS.h"
#include "logicWorker.h"
#include "subtitleqDecUserLogic.h"

#include "hPlayerWorkerMgr.h"

#include "playerDataRpc.h"

static int sprocSubtitleqDecExitNtfAsk (subtitleqDecUserLogic& rLogic, subtitleqDec& rServer)
{
    rLogic.clean();
    rLogic.setState(subtitleqDecUserLogic::subtitleqLogicState_willExit);
    subtitleqDecExitOKNtfAskMsg  msg;
    rServer.sendMsg(msg);
	gInfo("Rec procSubtitleqDecExitNtfAsk");
    return procPacketFunRetType_del;
}
int  subtitleqDec::procSubtitleqDecExitNtfAsk ()
{
    return sprocSubtitleqDecExitNtfAsk(*(dynamic_cast<subtitleqDecUserLogic*>(getIUserLogicWorker ())), *this);
}
