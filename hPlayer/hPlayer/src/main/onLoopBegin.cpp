#include "main.h"
#include "gLog.h"
#include "mainLogic.h"

int main::onLoopBegin()
{
    static mainLogic sLogic(*this);
    setUserData (&sLogic);
    auto nRet = sLogic.onLoopBegin();
    return nRet;
}

