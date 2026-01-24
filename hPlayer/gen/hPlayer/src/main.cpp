#include "comFun.h"
#include "strFun.h"
#include <string.h>
#include <memory>
#include <thread>
#include <chrono>
#include <vector>
#include <map>
#include <set>
#include <iostream>
#include <sstream>
#include <string>
#include "myAssert.h"
#include "loop.h"
#include "modelLoder.h"

int  beginMain(int argC, char** argV);
void endMain();
using defArgMap = std::map<std::string, std::string>;
void addDefKV(defArgMap& rMap, const char* szKV, const char delim = '=')
{
	auto ret = stringSplit (szKV, delim);
	if (ret.size() == 2) {
		rMap[ret[0]] = ret[1];
	}
}

int main(int cArg, char** argS)
{
	auto& ws = std::cout;
	ws<<"start main"<<std::endl;
	int nRet = 0;
	nRet = beginMain(cArg, argS);

    typedef void (*afterAllLoopEndBeforeExitAppFT)();
    afterAllLoopEndBeforeExitAppFT funAfterAllLoopEndBeforeExitApp = nullptr;
	do {
		if (nRet) {
			ws<<" beginMain error nRet = "<<nRet<<std::endl;
			break;
		}
		defArgMap aDefArgMap;
		addDefKV(aDefArgMap, "procId=0");
		addDefKV(aDefArgMap, "runWorkNum=worker:0*1*0*10-1*1*1*10");
		addDefKV(aDefArgMap, "netNum=4");
		addDefKV(aDefArgMap, "modelName=hPlayerModule");
		addDefKV(aDefArgMap, "appNetType=0");
		addDefKV(aDefArgMap, "appGroupId =0");
		addDefKV(aDefArgMap, "projectInstallDir=C:/work/hPlayer/hPlayerInstall");
		addDefKV(aDefArgMap, "logFile=hPlayer.log");
		addDefKV(aDefArgMap, "level0=cppLevel0L ");
		addDefKV(aDefArgMap, "frameHome=C:/work/firstBuildInstall");
		addDefKV(aDefArgMap, "frameConfigFile=hPlayerframeConfig.txt");

		std::string strDefDelim;
		char defDelim ='#';
		getOnceValueFromArgS (cArg, argS, "defDelim", strDefDelim);
		if (!strDefDelim.empty()) {
			defDelim = strDefDelim.c_str()[0];
		}
		for (decltype (cArg) i = 1; i < cArg; i++) {
			addDefKV (aDefArgMap, argS[i], defDelim);
		}
		int defArgLen = 0;
		for (auto it = aDefArgMap.begin (); it != aDefArgMap.end (); it++) {
			defArgLen += it->first.length();
			defArgLen += it->second.length();
			defArgLen += 2;
		}
		auto nDefArg = (int)(aDefArgMap.size());
		auto defArgS = std::make_unique<char*[]>(nDefArg + 1);
		auto defArgTxt = std::make_unique<char[]>(defArgLen);
		int curDef = 0;
		char* pCur = defArgTxt.get();
		for (auto it = aDefArgMap.begin (); it != aDefArgMap.end (); it++) {
			defArgS[curDef++] = pCur;
			pCur += (sprintf (pCur, "%s=%s", it->first.c_str(), it->second.c_str()) + 1);
		}

		std::string pLevel0Name;
		std::string strFrameHome;

		auto level0Ret = getTwoValueFromArgS (cArg, argS,  "level0", "frameHome", pLevel0Name, strFrameHome);
		if (!level0Ret) {
			level0Ret = getTwoValueFromArgS (nDefArg, defArgS.get(),  "level0", "frameHome", pLevel0Name, strFrameHome);
		}
		
		if (pLevel0Name.empty()) {
			std::cout<<"LevelName empty"<<std::endl;
			nRet = 1;
		} else {
			std::string strDll;
			if (!strFrameHome.empty()) {
				strDll += strFrameHome;
				strDll += "/";
			}
			strDll += getDllPath (pLevel0Name.c_str());
			auto handle = loadDll (strDll.c_str());
			myAssert(handle);
			do {
				if(!handle) {
					nRet = 11;
					ws<<"load module "<<strDll<<" error"<<std::endl;
					break;
				}
				typedef int (*initFunType) (int cArg, char** argS, int cDArg, char** argDS);
				auto funOnLoad = (initFunType)(getFun(handle, "initFun"));
				myAssert(funOnLoad);
				if(!funOnLoad) {
					std::cout<<"Level0 initFun empty error is"<<std::endl;
					nRet = 12;
					break;
				}
				auto nnR = funOnLoad (cArg, argS, nDefArg, defArgS.get());
				if (nnR) {
					std::cout<<"funOnLoad error nnR = "<<nnR<<std::endl;
					break;
				}
				defArgTxt.reset ();
				defArgS.reset();
				typedef int (*loopBeginFT)(loopHandleType pThis);
	auto funLoopBegin = (loopBeginFT)(getFun(handle, "onPhyLoopBegin"));
	typedef int (*loopEndFT)(loopHandleType pThis);
	auto funLoopEnd = (loopEndFT)(getFun(handle, "onPhyLoopEnd"));
	typedef bool (*loopFrameFT)(loopHandleType pThis);
	auto funLoopFrame = (loopFrameFT)(getFun(handle, "onPhyFrame"));

	typedef bool (*loopFrameFT)(loopHandleType pThis);

	typedef int  (*runThNumFT) (char*, int);
	auto funRunThNum = (runThNumFT)(getFun(handle, "onPhyGetRunThreadIdS"));
	
	typedef int (*getServerGroupInfoFT)(uword, ubyte*, ubyte*);
	auto funGetServerGroupInfo = (getServerGroupInfoFT)(getFun(handle, "getServerGroupInfo"));
    funAfterAllLoopEndBeforeExitApp = (afterAllLoopEndBeforeExitAppFT)(getFun(handle, "afterAllLoopEndBeforeExitApp"));

	ubyte beginId = 0;
	ubyte runNum = 0;
	auto  getGroupInfoRet = funGetServerGroupInfo (0, &beginId, &runNum);
	myAssert (0 == getGroupInfoRet);

	auto nBegRet = funLoopBegin (beginId);
	if (!nBegRet) {
		while(1) {
			auto bE = funLoopFrame (beginId);
			if (bE) {
				break;
			}
		}
	}
	
	funLoopEnd (beginId);
	int curRunNum = 0;
	const auto c_tempSize = 256;
	auto tempBuf = std::make_unique<char[]>(c_tempSize);
	auto pTemp = tempBuf.get();
	do {
		curRunNum = funRunThNum (pTemp, c_tempSize);
		if (curRunNum) {
			std::cout<<" leaf run num is: "<<curRunNum<<" run serverS is : "<<pTemp<<std::endl;
			std::this_thread::sleep_for(std::chrono::microseconds (500000));
		}
	} while (curRunNum);
					
				std::cout<<pTemp<<std::endl;
			}while(0);
		}
    if (funAfterAllLoopEndBeforeExitApp) {
        funAfterAllLoopEndBeforeExitApp();
    }
		endMain();
	} while (0);
	std::this_thread::sleep_for(std::chrono::microseconds (1000000));
	std::cout<<" app exit now"<<std::endl;
	return nRet;
}