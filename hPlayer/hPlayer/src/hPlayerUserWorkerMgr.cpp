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

#include <SDL.h>
#include "hPlayerConfig.h"
#include "ffplayCom.h"

int ffplayMain(int argc, char **argv);
int hPlayerUserWorkerMgr::initLogicUser (int cArg, char** argS, ForLogicFun* pForLogic, int cDefArg, char** defArgS)
{
    int nRet = 0;
    do {
        nRet = ffplayMain (cArg, argS);
        break;
        auto& rUserConfig = tSingleton<hPlayerConfig>::single ();
        std::string strFileName;
        auto playFile = rUserConfig.playFile ();
        if (playFile) {
            if (strlen(playFile)) {
                strFileName = playFile;
            }
        }
        if (strFileName.empty()) {
            gError(" have no play file");
            nRet = 1;
            break;
        }
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
            gError("SDL_Init failed: " << SDL_GetError());
            nRet = 2;
            break;
        }
    } while(0);
    return nRet;
}

void hPlayerUserWorkerMgr::onAppExit()
{
    do_exit (getVideoState());
    // SDL_Quit();
}
