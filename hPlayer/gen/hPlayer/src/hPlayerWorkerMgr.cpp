#include "hPlayerWorkerMgr.h"
#include "logicFrameConfig.h"
#include "tSingleton.h"
#include "hPlayerConfig.h"
#include "rpcInfo.h"

int hPlayerWorkerMgr::initLogicGen (int cArg, char** argS, ForLogicFun* pForLogic, int cDefArg, char** defArgS)
{
	auto& rConfig = tSingleton<logicFrameConfig>::single ();
    tSingleton<hPlayerConfig>::createSingleton ();
    auto& rUserConfig = tSingleton<hPlayerConfig>::single ();
	int nRet = 0;
	do {
        int nR = rUserConfig.procCmdArgS (cDefArg, defArgS);
        if (nR) {
            nRet = 1;
            break;
        }
        nR = rUserConfig.procCmdArgS (cArg, argS);
        if (nR) {
            nRet = 2;
            break;
        }

        std::string userConfigFile = rConfig.projectInstallDir();
        std::string strUserConfigFile = "userConfig.txt";
        auto szUserConfigFile = rConfig.userConfigFile ();
        if (szUserConfigFile) {
            if (strlen(szUserConfigFile)) {
                strUserConfigFile = szUserConfigFile;
            }
        }
		userConfigFile += "/config/";

		userConfigFile += strUserConfigFile;
		rUserConfig.loadConfig (userConfigFile.c_str());
		nR = rUserConfig.procCmdArgS (cArg, argS);

		auto serverGroupNum = rConfig.serverGroupNum ();
		auto serverGroups = rConfig.serverGroups ();
		for (decltype (serverGroupNum) i = 0; i < serverGroupNum; i++) {
			if (i == 0) {
				m_main.first = std::make_unique<main[]>(serverGroups[i].runNum);
				for (decltype (serverGroups[i].runNum) j = 0; j < serverGroups[i].runNum; j++) {
					m_allServers [serverGroups[i].beginId + j] = m_main.first.get() + j;
				}
			} else if (i == 1) {
				m_decoTh.first = std::make_unique<decoTh[]>(serverGroups[i].runNum);
				for (decltype (serverGroups[i].runNum) j = 0; j < serverGroups[i].runNum; j++) {
					m_allServers [serverGroups[i].beginId + j] = m_decoTh.first.get() + j;
				}
			} 
		}
		
		const uword maxPairNum = 0x2000;
		const uword maxMsgNum = maxPairNum * 2;
		auto tempBuf = std::make_unique<uword[]>(maxMsgNum);
		nR = getDefProc (tempBuf.get(), maxMsgNum);
		myAssert (nR < maxMsgNum);
		nR = m_defProcMap.init((defProcMap::NodeType*)(tempBuf.get()), nR/2);
		myAssert (0 == nR);
			
		if (initLogicUser(cArg, argS, pForLogic, cDefArg, defArgS)) {
            nRet = 9;
            break;
       }
	} while (0);
	return nRet;
}

