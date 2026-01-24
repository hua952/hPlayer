#include "main.h"

int main::onWorkerInitGen(ForLogicFun* pForLogic)
{

	int regMainProcPacketFun (regMsgFT fnRegMsg, ServerIDType serId);
	regMainProcPacketFun(pForLogic->fnRegMsg, serverId ());
	auto setAttrFun = pForLogic->fnSetAttr;
	auto nRet = onWorkerInit(pForLogic);
	
	return nRet;
}

