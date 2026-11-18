#include "tSingleton.h"
#include "msg.h"
#include "gLog.h"
#include "myAssert.h"
#include <memory>
#include "loopHandleS.h"
#include "logicWorker.h"
#include "mainUserLogic.h"

#include "hPlayerWorkerMgr.h"

#include "playerDataRpc.h"

static int sprocAudioUpdateToMainAsk (mainUserLogic& rLogic, main& rServer, const audioUpdateToMainAsk& rAsk )
{
	gInfo("Rec procAudioUpdateToMainAsk");
    rLogic.setAudioChannels (rAsk.m_audioChannels);
    rLogic.setAudioFreq(rAsk.m_audioFreq);
    return procPacketFunRetType_del;
}
int  main::procAudioUpdateToMainAsk (const audioUpdateToMainAsk& rAsk )
{
    return sprocAudioUpdateToMainAsk(*(dynamic_cast<mainUserLogic*>(getIUserLogicWorker ())), *this,rAsk);
}
