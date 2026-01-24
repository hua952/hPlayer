#include "hPlayerConfig.h"
#include <memory>
#include <sstream>
#include <fstream>
#include <vector>
#include "strFun.h"

hPlayerConfig::hPlayerConfig ()
{
	strCpy("", m_modelS);
	strCpy("", m_playFile);
	
}
const char*  hPlayerConfig::modelS ()
{
    return m_modelS.get();
}

void  hPlayerConfig::setModelS (const char* v)
{
	strCpy(v, m_modelS);
}

const char*  hPlayerConfig::playFile ()
{
    return m_playFile.get();
}

void  hPlayerConfig::setPlayFile (const char* v)
{
	strCpy(v, m_playFile);
}




int  hPlayerConfig:: dumpConfig (const char* szFile)
{
	int nRet = 0;
	
	std::unique_ptr<char[]>	dirBuf;
	strCpy (szFile,dirBuf);
	auto pDir = dirBuf.get();
	upDir (pDir);
	do {
		std::ofstream ofs (szFile);
		if (!ofs) {
			nRet = 1;
			break;
		}
	int nmodelSLen = 0;
	auto modelS = this->modelS();
	if (modelS) nmodelSLen = strlen(modelS);
	std::string strTmodelS =R"("")";
	if (nmodelSLen) 
		strTmodelS = modelS;
		ofs<<"modelS="<<strTmodelS<<std::endl;
	int nplayFileLen = 0;
	auto playFile = this->playFile();
	if (playFile) nplayFileLen = strlen(playFile);
	std::string strTplayFile =R"("")";
	if (nplayFileLen) 
		strTplayFile = playFile;
		ofs<<"playFile="<<strTplayFile<<std::endl;

	} while (0);
	return nRet;
}

int  hPlayerConfig:: loadConfig (const char* szFile)
{
	int nRet = 0;
	do {
		std::ifstream ifs (szFile);
		if (!ifs) {
			dumpConfig (szFile);
			break;
		}

		std::stringstream ss;
		std::string strLine;
		std::vector <std::string> vecT;
		while(getline(ifs, strLine)) {
			auto nf = strLine.find ("##");
			if (nf != strLine.npos) {
				strLine = strLine.substr(0, nf);
			}
			vecT.push_back (strLine);
		}
		auto argS = std::make_unique<std::unique_ptr<char[]>[]> (vecT.size());
		auto ppArgS = std::make_unique<char*[]> (vecT.size());
		auto n = 0;
		for (auto it = vecT.begin (); it != vecT.end (); it++) {
			auto& arg = argS[n];
			strCpy (it->c_str(), arg);
			ppArgS[n] = arg.get();
			n++;
		}
		nRet = procCmdArgS (n, ppArgS.get());
	} while (0);
	return nRet;
}

int  hPlayerConfig:: procCmdArgS (int nArg, char** argS)
{
	int nRet = 0;
	do {
		for (decltype (nArg) i = 0; i < nArg; i++) {
			std::unique_ptr<char[]> pArg;
			strCpy (argS[i], pArg);
			char* retS[3];
			auto pBuf = pArg.get ();
			auto nR = strR (pBuf, '=', retS, 3);
			if (2 != nR) {
				continue;
			}
			std::string strKey;
			std::string strVal;
			std::stringstream ssK (retS[0]);
			ssK>>strKey;
			std::stringstream ssV (retS[1]);
		if (strKey == "modelS") {
				ssV>>strVal;
				if('"'==strVal.c_str()[0] && '"'==strVal.c_str()[strVal.length()-1]) {
					strVal = strVal.substr(1,strVal.length()-2);
				}
	strCpy(strVal.c_str(), m_modelS);
				continue;
			}
				if (strKey == "playFile") {
				ssV>>strVal;
				if('"'==strVal.c_str()[0] && '"'==strVal.c_str()[strVal.length()-1]) {
					strVal = strVal.substr(1,strVal.length()-2);
				}
	strCpy(strVal.c_str(), m_playFile);
				continue;
			}
		
		}
	} while (0);
	return nRet;
}
