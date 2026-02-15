#include "audioDec.h"
#include "audioDecUserLogic.h"

int audioDec::onWorkerInit(ForLogicFun* pForLogic)
{
	int nRet = 0;
	do {
        m_pIUserLogicWorker = std::make_unique<audioDecUserLogic> (*this);
	} while (0);
	return nRet;
}

