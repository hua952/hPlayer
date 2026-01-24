
#include "msgGroupId.h"
#include "playerDataMsgId.h"
#include "playerDataRpc.h"

packetHead* allocPacket(udword udwS);
packetHead* allocPacketExt(udword udwS, udword ExtNum);
seekPosAskMsg:: seekPosAskMsg ()
{
	m_pPacket = (packetHead*)allocPacket(sizeof (seekPosAsk));
	netPacketHead* pN = P2NHead(m_pPacket);
	NSetAddSer(pN);
	pN->uwMsgID = playerData2FullMsg(playerDataMsgId_seekPosAsk);
	
}
seekPosAskMsg:: seekPosAskMsg (packetHead* p):CMsgBase(p)
{
}
seekPosAsk* seekPosAskMsg:: pack()
{
    return ((seekPosAsk*)(P2User(m_pPacket)));
}

seekPosRetMsg:: seekPosRetMsg ()
{
	m_pPacket = (packetHead*)allocPacket(sizeof (seekPosRet));
	netPacketHead* pN = P2NHead(m_pPacket);
	NSetAddSer(pN);
	pN->uwMsgID = playerData2FullMsg(playerDataMsgId_seekPosRet);
	NSetRet(pN);
	
}
seekPosRetMsg:: seekPosRetMsg (packetHead* p):CMsgBase(p)
{
}
seekPosRet* seekPosRetMsg:: pack()
{
    return ((seekPosRet*)(P2User(m_pPacket)));
}

playURLAskMsg:: playURLAskMsg ()
{
	m_pPacket = (packetHead*)allocPacket(sizeof (playURLAsk));
	netPacketHead* pN = P2NHead(m_pPacket);
	NSetAddSer(pN);
	pN->uwMsgID = playerData2FullMsg(playerDataMsgId_playURLAsk);
	auto p = ((playURLAsk*)(N2User(pN)));
	p->m_urlNum = 0;
	playURLAsk* p2 = ((playURLAsk*)(N2User(pN)));
	p2->m_url[0] = 0;
	
}
playURLAskMsg:: playURLAskMsg (packetHead* p):CMsgBase(p)
{
}
playURLAsk* playURLAskMsg:: pack()
{
    return ((playURLAsk*)(P2User(m_pPacket)));
}

playURLRetMsg:: playURLRetMsg ()
{
	m_pPacket = (packetHead*)allocPacket(sizeof (playURLRet));
	netPacketHead* pN = P2NHead(m_pPacket);
	NSetAddSer(pN);
	pN->uwMsgID = playerData2FullMsg(playerDataMsgId_playURLRet);
	NSetRet(pN);
	
}
playURLRetMsg:: playURLRetMsg (packetHead* p):CMsgBase(p)
{
}
playURLRet* playURLRetMsg:: pack()
{
    return ((playURLRet*)(P2User(m_pPacket)));
}

