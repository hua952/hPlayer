#ifndef _exportFun_h__
#define _exportFun_h__
#include "mainLoop.h"

extern "C"
{
	dword afterLoad(int nArgC, char** argS, ForLogicFun* pForLogic, int nDefArgC, char** defArgS, char* taskBuf, int taskBufSize);
	int onLoopBegin	(serverIdType	fId);
	int onFrameLogic	(serverIdType	fId);
	void onLoopEnd	(serverIdType	fId);
	void  beforeUnload();
	int   onRecPacket(serverIdType	fId, packetHead* pack);
}
#endif