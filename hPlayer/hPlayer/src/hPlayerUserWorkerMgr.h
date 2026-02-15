#include <iostream>
#include "logicWorkerMgr.h"

class hPlayerUserWorkerMgr:public IUserLogicWorkerMgr
{
public:
    int initLogicUser (int cArg, char** argS, ForLogicFun* pForLogic, int cDefArg, char** defArgS);
    void onAppExit() override;
};
