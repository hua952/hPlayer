#include "main.h"
#include "mainLogic.h"

int main::onLoopEnd()
{
    auto pLogic = (mainLogic*)(userData());
    return pLogic->onLoopEnd();
}

