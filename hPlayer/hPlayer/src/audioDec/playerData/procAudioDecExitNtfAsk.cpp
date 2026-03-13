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

static int sprocAudioDecExitNtfAsk (audioDecUserLogic& rLogic, audioDec& rServer)
{
    rLogic.clean();
    rLogic.setState(audioDecUserLogic::audioLogicState_willExit);
    audioDecExitOKNtfAskMsg ask;
    rServer.sendMsg(ask);
	gInfo("Rec procAudioDecExitNtfAsk");
    return procPacketFunRetType_del;
}
int  audioDec::procAudioDecExitNtfAsk ()
{
    return sprocAudioDecExitNtfAsk(*(dynamic_cast<audioDecUserLogic*>(getIUserLogicWorker ())), *this);
}
