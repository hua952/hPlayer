#ifndef _msgGroupId_h__
#define  _msgGroupId_h__

enum msgGroupId
{
    msgGroupId_playerData = 0,
};

#define playerData2FullMsg(p) ((uword)((msgGroupId_playerData * 100)+p))

#endif