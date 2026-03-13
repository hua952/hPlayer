#include "tSingleton.h"
#include "msg.h"
#include "gLog.h"
#include "myAssert.h"
#include <memory>
#include "loopHandleS.h"
#include "logicWorker.h"
#include "audioDecUserLogic.h"

#include "hPlayerWorkerMgr.h"

#include "playerDataRpc.h"

static int sprocInitAudioAsk (audioDecUserLogic& rLogic, audioDec& rServer)
{
    rLogic.setState(audioDecUserLogic::audioLogicState_thisNeetInit);
	gInfo("Rec procInitAudioAsk");
    return procPacketFunRetType_del;
}
int  audioDec::procInitAudioAsk ()
{
    return sprocInitAudioAsk(*(dynamic_cast<audioDecUserLogic*>(getIUserLogicWorker ())), *this);
}
