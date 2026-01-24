#include <iostream>
#include <cstring>
#include <string>
#include "exportFun.h"
#include "msg.h"
#include "myAssert.h"
#include "logicWorker.h"
#include "logicWorkerMgr.h"
#include "tSingleton.h"
#include "gLog.h"
#include "hPlayerWorkerMgr.h"

dword afterLoad(int nArgC, char** argS, ForLogicFun* pForLogic, int nDefArgC, char** defArgS, char* taskBuf, int taskBufSize)
{
	setForMsgModuleFunS (pForLogic);
	tSingleton<hPlayerWorkerMgr>::createSingleton();
	auto &rMgr = tSingleton<hPlayerWorkerMgr>::single();
	logicWorkerMgr::s_mgr = &rMgr;
	return rMgr.initLogicWorkerMgr (nArgC, argS, pForLogic, nDefArgC, defArgS, taskBuf, taskBufSize);
}

int onLoopBegin	(serverIdType	fId)
{
	int nRet = 1;
	auto &rMgr = tSingleton<hPlayerWorkerMgr>::single();
	auto pS = rMgr.findServer(fId);
	if (pS) {
		nRet = pS->onLoopBeginBase	();
	}
	return nRet;
}

int onFrameLogic	(serverIdType	fId)
{
	int nRet = procPacketFunRetType_del;
	auto &rMgr = tSingleton<hPlayerWorkerMgr>::single();
	auto pS = rMgr.findServer(fId);
	if (pS) {
		if (pS->willExit()) {
			nRet = procPacketFunRetType_exitNow;
		} else {
			nRet = pS->onLoopFrameBase();
		}
	}
	return nRet;
}

void onLoopEnd	(serverIdType	fId)
{
	auto &rMgr = tSingleton<hPlayerWorkerMgr>::single();
	auto pS = rMgr.findServer(fId);
	if (pS) {
		pS->onLoopEndBase();
	}
}

void  beforeUnload()
{
	auto &rMgr = tSingleton<hPlayerWorkerMgr>::single();
    rMgr.onAppExit();
	std::cout<<"In   beforeUnload"<<std::endl;
}
int   onRecPacket(serverIdType	fId, packetHead* pack)
{
	return tSingleton<hPlayerWorkerMgr>::single().processOncePack (fId, pack);
}
