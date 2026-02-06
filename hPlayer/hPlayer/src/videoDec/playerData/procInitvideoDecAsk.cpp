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

void  videoDec::procInitvideoDecAsk ()
{
    auto pLogic = (videoDecLogic *)(userData());
    pLogic->setState(videoDecLogic::videoDecLogicState_thisNeetInit);
	gInfo("Rec procInitvideoDecAsk");
}
