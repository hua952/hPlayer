#include "tSingleton.h"
#include "msg.h"
#include "gLog.h"
#include "myAssert.h"
#include <memory>
#include "loopHandleS.h"
#include "logicWorker.h"
#include "hPlayerWorkerMgr.h"

#include "playerDataRpc.h"
#include "audioLogic.h"

void  audioDec::procAudioDecExitNtfAsk ()
{
    auto pLogic = (audioLogic*)(userData());
    pLogic->clean();
    pLogic->setState(audioLogic::audioLogicState_willExit);
    audioDecExitOKNtfAskMsg ask;
    pLogic->getAudioDec().sendMsg(ask);
	gInfo("Rec procAudioDecExitNtfAsk");
}
