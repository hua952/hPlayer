#include "tSingleton.h"
#include "msg.h"
#include "gLog.h"
#include "myAssert.h"
#include <memory>
#include "loopHandleS.h"
#include "logicWorker.h"
#include "hPlayerWorkerMgr.h"
#include "videoDecLogic.h"
#include "playerDataRpc.h"

void  videoDec::procVideoDecExitNtfAsk ()
{
    auto pLogic = (videoDecLogic *)(userData());
    pLogic->clean();
    pLogic->setState(videoDecLogic::videoDecLogicState_willExit);
    videoDecExitOKNtfAskMsg  msg;
    auto& th = pLogic->getVideoDec();
    th.sendMsg(msg);
	gInfo("Rec procVideoDecExitNtfAsk");
}
