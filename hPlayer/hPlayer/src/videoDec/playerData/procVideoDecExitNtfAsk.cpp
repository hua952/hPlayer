#include "tSingleton.h"
#include "msg.h"
#include "gLog.h"
#include "myAssert.h"
#include <memory>
#include "loopHandleS.h"
#include "logicWorker.h"
#include "videoDecUserLogic.h"

#include "hPlayerWorkerMgr.h"

#include "playerDataRpc.h"

static int sprocVideoDecExitNtfAsk (videoDecUserLogic& rLogic, videoDec& rServer)
{
    rLogic.clean();
    rLogic.setState(videoDecUserLogic::videoDecLogicState_willExit);
    videoDecExitOKNtfAskMsg  msg;
    rServer.sendMsg(msg);
	gInfo("Rec procVideoDecExitNtfAsk");
    return procPacketFunRetType_del;
}
int  videoDec::procVideoDecExitNtfAsk ()
{
    return sprocVideoDecExitNtfAsk(*(dynamic_cast<videoDecUserLogic*>(getIUserLogicWorker ())), *this);
}
