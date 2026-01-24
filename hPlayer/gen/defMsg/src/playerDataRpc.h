#ifndef _playerDataRpch__
#define _playerDataRpch__
#include "msgStruct.h"

struct seekPosAsk
{
    double    m_pos;
};
class seekPosAskMsg :public CMsgBase
{
public:
	seekPosAskMsg ();
	seekPosAskMsg (packetHead* p);
	seekPosAsk* pack ();
    
};

struct seekPosRet
{
    double    m_pos;
};
class seekPosRetMsg :public CMsgBase
{
public:
	seekPosRetMsg ();
	seekPosRetMsg (packetHead* p);
	seekPosRet* pack ();
    
};

struct playURLAsk
{
    udword    m_urlNum;
    char    m_url [512];/* 文件名或网址 */
};
class playURLAskMsg :public CMsgBase
{
public:
	playURLAskMsg ();
	playURLAskMsg (packetHead* p);
	playURLAsk* pack ();
    
};

struct playURLRet
{
    double    m_totalDuration;
    udword    m_result;
};
class playURLRetMsg :public CMsgBase
{
public:
	playURLRetMsg ();
	playURLRetMsg (packetHead* p);
	playURLRet* pack ();
    
};
#endif
