#include "msgGroupId.h"
#include "msgStruct.h"
#include "logicFrameConfig.h"
#include "tSingleton.h"
#include "mainLoop.h"
#include "hPlayerWorkerMgr.h"
#include "playerDataMsgId.h"
#include "playerDataRpc.h"


static int onPlayURLAsk(pPacketHead pAsk, pPacketHead& pRet, procPacketArg* pArg)
{
	int nRet = procPacketFunRetType_del;
    playURLAskMsg ask (pAsk);
	pAsk = ask.getPack ();
	playURLRetMsg ret;
    
	auto&  workerMgr = tSingleton<hPlayerWorkerMgr>::single();
	auto allServerNum = workerMgr.allServerNum ();
	myAssert (pArg->handle < allServerNum);
	auto pServer = workerMgr.allServers()[pArg->handle];
	auto pRealServer = dynamic_cast<decoTh*>(pServer);
	myAssert (pRealServer);
	pRealServer->procPlayURLAsk(*ask.pack(), *ret.pack());
	pRet = ret.pop();
		
	ask.pop ();
	return nRet;
}

static int onSeekPosAsk(pPacketHead pAsk, pPacketHead& pRet, procPacketArg* pArg)
{
	int nRet = procPacketFunRetType_del;
    seekPosAskMsg ask (pAsk);
	pAsk = ask.getPack ();
	seekPosRetMsg ret;
    
	auto&  workerMgr = tSingleton<hPlayerWorkerMgr>::single();
	auto allServerNum = workerMgr.allServerNum ();
	myAssert (pArg->handle < allServerNum);
	auto pServer = workerMgr.allServers()[pArg->handle];
	auto pRealServer = dynamic_cast<decoTh*>(pServer);
	myAssert (pRealServer);
	pRealServer->procSeekPosAsk(*ask.pack(), *ret.pack());
	pRet = ret.pop();
		
	ask.pop ();
	return nRet;
}

int regDecoThProcPacketFun (regMsgFT fnRegMsg, ServerIDType serId)
{
	int nRet = 0;
    fnRegMsg (serId, playerData2FullMsg(playerDataMsgId_playURLAsk), onPlayURLAsk);
    fnRegMsg (serId, playerData2FullMsg(playerDataMsgId_seekPosAsk), onSeekPosAsk);
    return nRet;
}

