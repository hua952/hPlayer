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

static int sprocSubtitleqAsk (subtitleqDecUserLogic& rLogic, subtitleqDec& rServer)
{
    rLogic.setState(subtitleqDecUserLogic::subtitleqLogicState_thisNeetInit);
	gInfo("Rec procSubtitleqAsk");
    return procPacketFunRetType_del;
}
int  subtitleqDec::procSubtitleqAsk ()
{
    return sprocSubtitleqAsk(*(dynamic_cast<subtitleqDecUserLogic*>(getIUserLogicWorker ())), *this);
}
