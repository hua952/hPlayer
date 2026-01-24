#include "main.h"
#include "mainLogic.h"

int main::onLoopFrame()
{
    auto pLogic = (mainLogic*)(userData());
    auto nRet = pLogic->onLoopFrame();
    if (procPacketFunRetType_exitAfterLoop & nRet) {
        this->ntfOtherLocalServerExit ();
    }
    return nRet;
}

