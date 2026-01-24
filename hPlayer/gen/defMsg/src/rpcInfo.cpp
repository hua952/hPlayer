#include "msgGroupId.h"
#include "msg.h"
#include <fstream>
#include <map>
#include "rpcInfo.h"
#include "myAssert.h"
#include "loopHandleS.h"
#include "playerDataMsgId.h"
#include "playerDataRpc.h"



void dumpStructS ()
{
	std::ofstream os("dumpStructS.txt");
	os<<"seekPosAsk.m_pos    "<<(uqword)&(((seekPosAsk*)0)->m_pos)<<std::endl;
	os<<"seekPosRet.m_pos    "<<(uqword)&(((seekPosRet*)0)->m_pos)<<std::endl;
	os<<"playURLAsk.m_urlNum    "<<(uqword)&(((playURLAsk*)0)->m_urlNum)<<std::endl;
	os<<"playURLAsk.m_url    "<<(uqword)(((playURLAsk*)0)->m_url)<<std::endl;
	os<<"playURLRet.m_totalDuration    "<<(uqword)&(((playURLRet*)0)->m_totalDuration)<<std::endl;
	os<<"playURLRet.m_result    "<<(uqword)&(((playURLRet*)0)->m_result)<<std::endl;

}

int  checkStructS (const char* szFile)
{
	int nRet = 0;
	do {
		uword ut = 0x201;
		auto pB = (ubyte*)&ut;
		if (1 != pB[0] || 2 != pB[1]) {
			nRet = 1;
			break;
		}
		std::ifstream ifs(szFile);
		if (!ifs) {
			nRet = 2;
			break;
		}
		std::map<std::string, uqword> tempMap;
		std::string strLine;
		while (std::getline (ifs, strLine)) {
			std::stringstream ts (strLine);
			std::string strKey;
			uqword uqwValue;
			ts>>strKey>>uqwValue;
			auto inRet = tempMap.insert (std::make_pair(strKey, uqwValue));
			myAssert (inRet.second);
		}
		std::map<std::string, uqword>::iterator it;

			it = tempMap.find ("seekPosAsk.m_pos");
		if (it == tempMap.end ()) {
				nRet = 3;
				break;
			}
			if (it->second != (uqword)&(((seekPosAsk*)0)->m_pos)) {
				nRet = 4;
				break;
			}
				it = tempMap.find ("seekPosRet.m_pos");
		if (it == tempMap.end ()) {
				nRet = 3;
				break;
			}
			if (it->second != (uqword)&(((seekPosRet*)0)->m_pos)) {
				nRet = 4;
				break;
			}
				it = tempMap.find ("playURLAsk.m_urlNum");
		if (it == tempMap.end ()) {
				nRet = 3;
				break;
			}
			if (it->second != (uqword)&(((playURLAsk*)0)->m_urlNum)) {
				nRet = 4;
				break;
			}
				it = tempMap.find ("playURLAsk.m_url");
		if (it == tempMap.end ()) {
				nRet = 3;
				break;
			}
			if (it->second != (uqword)(((playURLAsk*)0)->m_url)) {
				nRet = 4;
				break;
			}
				it = tempMap.find ("playURLRet.m_totalDuration");
		if (it == tempMap.end ()) {
				nRet = 3;
				break;
			}
			if (it->second != (uqword)&(((playURLRet*)0)->m_totalDuration)) {
				nRet = 4;
				break;
			}
				it = tempMap.find ("playURLRet.m_result");
		if (it == tempMap.end ()) {
				nRet = 3;
				break;
			}
			if (it->second != (uqword)&(((playURLRet*)0)->m_result)) {
				nRet = 4;
				break;
			}
			
	} while (0);
	return nRet;
}

int getDefProc (loopHandleType* pBuff, int buffNum)
{
	int nRet = 0;
	do {
		if (nRet >= buffNum) {
			break;
		}
		pBuff[nRet++] = playerData2FullMsg(playerDataMsgId_seekPosAsk);
		if (nRet >= buffNum) {
			break;
		}
		pBuff[nRet++] = ((appTmpId_hPlayer<<8) + hPlayerServerTmpID_decoTh);
		if (nRet >= buffNum) {
			break;
		}
		pBuff[nRet++] = playerData2FullMsg(playerDataMsgId_seekPosRet);
		if (nRet >= buffNum) {
			break;
		}
		pBuff[nRet++] = ((appTmpId_hPlayer<<8) + hPlayerServerTmpID_main);
		if (nRet >= buffNum) {
			break;
		}
		pBuff[nRet++] = playerData2FullMsg(playerDataMsgId_playURLAsk);
		if (nRet >= buffNum) {
			break;
		}
		pBuff[nRet++] = ((appTmpId_hPlayer<<8) + hPlayerServerTmpID_decoTh);
		if (nRet >= buffNum) {
			break;
		}
		pBuff[nRet++] = playerData2FullMsg(playerDataMsgId_playURLRet);
		if (nRet >= buffNum) {
			break;
		}
		pBuff[nRet++] = ((appTmpId_hPlayer<<8) + hPlayerServerTmpID_main);
		if (nRet >= buffNum) {
			break;
		}
		
	} while (0);
	return nRet;
}