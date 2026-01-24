#ifndef _decoTh_h__
#define _decoTh_h__

#include <memory>
#include "mainLoop.h"
#include "logicWorker.h"
#include "playerDataRpc.h"


class decoTh:public logicWorker
{
public:
	int onWorkerInitGen(ForLogicFun* pForLogic) override;
	int onWorkerInit(ForLogicFun* pForLogic) override;
	int onLoopBegin() override;
	int onLoopEnd() override;
	int onLoopFrame() override;
	int onCreateChannelRet(const channelKey& rKey, udword result) override;
	    void procPlayURLAsk (const playURLAsk& rAsk , playURLRet& rRet);
    void procSeekPosAsk (const seekPosAsk& rAsk , seekPosRet& rRet);

private:
};
#endif
