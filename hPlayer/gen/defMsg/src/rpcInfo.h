#ifndef rpcInfo_h__
#define rpcInfo_h__
#include "msg.h"
#include "arrayMap.h"

void dumpStructS ();
int  checkStructS (const char* szFile);
// void regRpcS (const ForMsgModuleFunS* pForLogic);
int getDefProc (uword* pBuff, int buffNum);

#endif