#include "tSingleton.h"
#include "msg.h"
#include "gLog.h"
#include "myAssert.h"
#include <memory>
#include "loopHandleS.h"
#include "logicWorker.h"
#include "hPlayerWorkerMgr.h"

#include "playerDataRpc.h"
#include "ffmpegDecoder.h"

void  decoTh::procPlayURLAsk (const playURLAsk& rAsk , playURLRet& rRet)
{
	gInfo("Rec procPlayURLAsk");
    auto pLogic = (ffmpegDecoder*)(userData());
    rRet.m_result = pLogic->playFile(rAsk.m_url);
    if (!rRet.m_result) {
        rRet.m_totalDuration = pLogic->totalDuration ();
    }
}
