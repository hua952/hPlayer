#include "tSingleton.h"
#include "msg.h"
#include "gLog.h"
#include "myAssert.h"
#include <memory>
#include "loopHandleS.h"
#include "logicWorker.h"
#include "hPlayerWorkerMgr.h"

#include "playerDataRpc.h"
#include "readPackLogic.h"

void  decoTh::procMainInitOKAsk ()
{
    auto pLogic = (readPackLogic*)(userData());
    pLogic->setState(readPackLogic::readState_thisNeetInit);
	gInfo("Rec procMainInitOKAsk");
}
