#include "msgGroupId.h"
#include "msgStruct.h"
#include "logicFrameConfig.h"
#include "tSingleton.h"
#include "mainLoop.h"
#include "hPlayerWorkerMgr.h"
#include "playerDataMsgId.h"
#include "playerDataRpc.h"


static int onPlayURLRet(pPacketHead pAsk, pPacketHead& pRet, procPacketArg* pArg)
{
	int nRet = procPacketFunRetType_del;
    playURLAskMsg ask (pAsk);
	pAsk = ask.getPack ();
	playURLRetMsg ret (pRet);
		pRet = ret.getPack ();
	
	auto&  workerMgr = tSingleton<hPlayerWorkerMgr>::single();
	auto allServerNum = workerMgr.allServerNum ();
	myAssert (pArg->handle < allServerNum);
	auto pServer = workerMgr.allServers()[pArg->handle];
	auto pRealServer = dynamic_cast<main*>(pServer);
	myAssert (pRealServer);
	pRealServer->procPlayURLRet(*ask.pack(), *ret.pack());
	ret.pop ();

	ask.pop ();
	return nRet;
}

static int onSeekPosRet(pPacketHead pAsk, pPacketHead& pRet, procPacketArg* pArg)
{
	int nRet = procPacketFunRetType_del;
    seekPosAskMsg ask (pAsk);
	pAsk = ask.getPack ();
	seekPosRetMsg ret (pRet);
		pRet = ret.getPack ();
	
	auto&  workerMgr = tSingleton<hPlayerWorkerMgr>::single();
	auto allServerNum = workerMgr.allServerNum ();
	myAssert (pArg->handle < allServerNum);
	auto pServer = workerMgr.allServers()[pArg->handle];
	auto pRealServer = dynamic_cast<main*>(pServer);
	myAssert (pRealServer);
	pRealServer->procSeekPosRet(*ask.pack(), *ret.pack());
	ret.pop ();

	ask.pop ();
	return nRet;
}

int regMainProcPacketFun (regMsgFT fnRegMsg, ServerIDType serId)
{
	int nRet = 0;
    fnRegMsg (serId, playerData2FullMsg(playerDataMsgId_playURLRet), onPlayURLRet);
    fnRegMsg (serId, playerData2FullMsg(playerDataMsgId_seekPosRet), onSeekPosRet);
    return nRet;
}

