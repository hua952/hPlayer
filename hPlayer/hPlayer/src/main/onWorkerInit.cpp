#include "main.h"
#include "mainUserLogic.h"

int main::onWorkerInit(ForLogicFun* pForLogic)
{
	int nRet = 0;
	do {
        m_pIUserLogicWorker = std::make_unique<mainUserLogic> (*this);
	} while (0);
	return nRet;
}

