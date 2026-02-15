#include "videoDec.h"
#include "videoDecUserLogic.h"

int videoDec::onWorkerInit(ForLogicFun* pForLogic)
{
	int nRet = 0;
	do {
        m_pIUserLogicWorker = std::make_unique<videoDecUserLogic> (*this);
	} while (0);
	return nRet;
}

