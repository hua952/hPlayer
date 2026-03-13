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

static int sprocInitvideoDecAsk (videoDecUserLogic& rLogic, videoDec& rServer)
{

    rLogic.setState(videoDecUserLogic::videoDecLogicState_thisNeetInit);
	gInfo("Rec procInitvideoDecAsk");
    return procPacketFunRetType_del;
}
int  videoDec::procInitvideoDecAsk ()
{
    return sprocInitvideoDecAsk(*(dynamic_cast<videoDecUserLogic*>(getIUserLogicWorker ())), *this);
}
