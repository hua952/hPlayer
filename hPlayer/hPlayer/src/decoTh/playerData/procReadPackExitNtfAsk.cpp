#include "tSingleton.h"
#include "msg.h"
#include "gLog.h"
#include "myAssert.h"
#include <memory>
#include "loopHandleS.h"
#include "logicWorker.h"
#include "hPlayerWorkerMgr.h"
#include "readPackLogic.h"
#include "playerDataRpc.h"

static bool sendEmptyPack(void* pU)
{
    auto pLogic = (readPackLogic*)(pU);
    pLogic->sendEmptyAudioPack();
    return false;
}
void  decoTh::procReadPackExitNtfAsk ()
{
    auto pLogic = (readPackLogic*)(userData());
    // pLogic->sendExitNtfToSub();
    decoTh& rTh = pLogic->getDecoTh();
    videoDecExitNtfAskMsg msg;
    rTh.sendMsg(msg);
    // rTh.addTimer(50, sendEmptyPack, pLogic);
    gInfo("Rec procReadPackExitNtfAsk");
}
