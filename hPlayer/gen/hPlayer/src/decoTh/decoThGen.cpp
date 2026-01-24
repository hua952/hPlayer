#include "decoTh.h"

int decoTh::onWorkerInitGen(ForLogicFun* pForLogic)
{

	int regDecoThProcPacketFun (regMsgFT fnRegMsg, ServerIDType serId);
	regDecoThProcPacketFun(pForLogic->fnRegMsg, serverId ());
	auto setAttrFun = pForLogic->fnSetAttr;
	auto nRet = onWorkerInit(pForLogic);
	
	return nRet;
}

