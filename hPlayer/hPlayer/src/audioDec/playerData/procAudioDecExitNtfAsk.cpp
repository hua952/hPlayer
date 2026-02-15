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

void  audioDec::procAudioDecExitNtfAsk ()
{
	auto pLogic = dynamic_cast<audioDecUserLogic*>(getIUserLogicWorker ());
    pLogic->clean();
    pLogic->setState(audioDecUserLogic::audioLogicState_willExit);
    audioDecExitOKNtfAskMsg ask;
    pLogic->getServer().sendMsg(ask);
	gInfo("Rec procAudioDecExitNtfAsk");
}
