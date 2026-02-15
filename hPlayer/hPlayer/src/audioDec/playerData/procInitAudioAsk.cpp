#include "tSingleton.h"
#include "msg.h"
#include "gLog.h"
#include "myAssert.h"
#include <memory>
#include "loopHandleS.h"
#include "logicWorker.h"
#include "hPlayerWorkerMgr.h"

#include "playerDataRpc.h"
#include "audioDecUserLogic.h"

void  audioDec::procInitAudioAsk ()
{
	auto pLogic = dynamic_cast<audioDecUserLogic*>(getIUserLogicWorker ());
    pLogic->setState(audioDecUserLogic::audioLogicState_thisNeetInit);
	gInfo("Rec procInitAudioAsk");
}
