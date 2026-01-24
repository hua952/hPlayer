#ifndef _main_h__
#define _main_h__

#include <memory>
#include "mainLoop.h"
#include "logicWorker.h"
#include "playerDataRpc.h"


class main:public logicWorker
{
public:
	int onWorkerInitGen(ForLogicFun* pForLogic) override;
	int onWorkerInit(ForLogicFun* pForLogic) override;
	int onLoopBegin() override;
	int onLoopEnd() override;
	int onLoopFrame() override;
	int onCreateChannelRet(const channelKey& rKey, udword result) override;
	    void procPlayURLRet (const playURLAsk& rAsk, playURLRet& rRet);
    void procSeekPosRet (const seekPosAsk& rAsk, seekPosRet& rRet);

private:
};
#endif
