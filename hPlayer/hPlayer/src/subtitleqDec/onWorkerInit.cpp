#include "subtitleqDec.h"
#include "subtitleqDecUserLogic.h"

int subtitleqDec::onWorkerInit(ForLogicFun* pForLogic)
{
	int nRet = 0;
	do {
        m_pIUserLogicWorker = std::make_unique<subtitleqDecUserLogic> (*this);
	} while (0);
	return nRet;
}

