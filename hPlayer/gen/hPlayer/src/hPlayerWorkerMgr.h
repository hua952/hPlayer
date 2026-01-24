#ifndef _hPlayerWorkerMgr_h__
#define _hPlayerWorkerMgr_h__

#include <memory>
#include "mainLoop.h"
#include "logicWorkerMgr.h"
#include "main.h"
#include "decoTh.h"


class hPlayerWorkerMgr:public logicWorkerMgr
{
public:
	int initLogicGen (int cArg, char** argS, ForLogicFun* pForLogic, int cDefArg, char** defArgS) override;
    int initLogicUser (int cArg, char** argS, ForLogicFun* pForLogic, int cDefArg, char** defArgS);
    void onAppExit();
    std::pair<std::unique_ptr<main[]>, loopHandleType>      m_main;
    std::pair<std::unique_ptr<decoTh[]>, loopHandleType>      m_decoTh;

private:
};
#endif
