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

static int sprocReadPackExitNtfAsk (decoThUserLogic& rLogic, decoTh& rServer)
{
    /*
    readPackExitOKNtfAskMsg  msg;
    rServer.sendMsg(msg);
    */

    subtitleqDecExitNtfAskMsg msg;
    rServer.sendMsg(msg);
    // rLogic.sendEmptySubtitleqPack ();
    rLogic.setState (decoThUserLogic::readState_willExit);

	gInfo("Rec procReadPackExitNtfAsk");
    return procPacketFunRetType_del;
}
int  decoTh::procReadPackExitNtfAsk ()
{
    return sprocReadPackExitNtfAsk(*(dynamic_cast<decoThUserLogic*>(getIUserLogicWorker ())), *this);
}
