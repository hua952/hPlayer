#include "decoTh.h"
#include "decoThUserLogic.h"

int decoTh::onWorkerInit(ForLogicFun* pForLogic)
{
	int nRet = 0;
	do {
        m_pIUserLogicWorker = std::make_unique<decoThUserLogic> (*this);
	} while (0);
	return nRet;
}

