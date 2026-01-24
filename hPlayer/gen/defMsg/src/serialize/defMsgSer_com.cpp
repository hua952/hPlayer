#include "myAssert.h"
#include "strFun.h"
#include "rpcInfo.h"
#include <memory>
#include "playerDataMsgId.h"
#include "playerDataRpc.h"

packetHead* allocPacket(udword udwS);
packetHead* allocPacketExt(udword udwS, udword ExtNum);
