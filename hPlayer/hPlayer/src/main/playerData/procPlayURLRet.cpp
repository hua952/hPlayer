#include "tSingleton.h"
#include "msg.h"
#include "gLog.h"
#include "myAssert.h"
#include <memory>
#include "loopHandleS.h"
#include "logicWorker.h"
#include "hPlayerWorkerMgr.h"

#include "playerDataRpc.h"
#include "mainLogic.h"
#include "logicWorker.h"
#include "imGuiMgr.h"

void  main::procPlayURLRet (const playURLAsk& rAsk, playURLRet& rRet)
{
    if (0 == rRet.m_result) {
        auto pLogic = (mainLogic*)(userData());
        auto& imguiMgr = pLogic->imguiMgr ();
        imguiMgr.setTotalDuration (rRet.m_totalDuration);
    }
	gInfo("Rec procPlayURLRet result = "<<rRet.m_result);
}
