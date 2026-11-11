#ifndef _baseUserLogic_h__
#define _baseUserLogic_h__
#include <memory>

class logicWorker;
class baseUserLogic
{
public:
    baseUserLogic ();
    ~baseUserLogic ();
    virtual logicWorker&  worker() = 0;
private:
    int  m_serialCache;
};
class baseUserStreamLogic:public baseUserLogic
{
public:
    int  streamIndex ();
    void  setStreamIndex (int v);
private:
    int  m_streamIndex{-1};
};
#endif
