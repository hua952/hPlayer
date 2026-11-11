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

static int sprocInitAudioAsk (audioDecUserLogic& rLogic, audioDec& rServer, const initAudioAsk& rAsk )
{
	gInfo("Rec procInitAudioAsk");
    // rLogic.setState(audioDecUserLogic::audioLogicState_thisNeetInit);
    rLogic.initAudio(rAsk.m_streamIndex);
    return procPacketFunRetType_del;
}
int  audioDec::procInitAudioAsk (const initAudioAsk& rAsk )
{
    return sprocInitAudioAsk(*(dynamic_cast<audioDecUserLogic*>(getIUserLogicWorker ())), *this,rAsk);
}
