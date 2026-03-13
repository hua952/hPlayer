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
#include "hPlayerUserWorkerMgr.h"

extern "C"
{
    #include "ffplayCom.h"
}

int hPlayerUserWorkerMgr::initLogicUser (int cArg, char** argS, ForLogicFun* pForLogic, int cDefArg, char** defArgS)
{
    int nRet = 0;
    do {
        ffplayMain (cArg, argS);
    } while(0);
    return nRet;
}

void hPlayerUserWorkerMgr::onAppExit()
{
    do_exit (getVideoState());
}
