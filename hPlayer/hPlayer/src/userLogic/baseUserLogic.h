#ifndef _baseUserLogic_h__
#define _baseUserLogic_h__
#include <memory>

class logicWorker;
class baseUserLogic
{
public:
    baseUserLogic ();
    ~baseUserLogic ();
    int  streamIndex ();
    void  setStreamIndex (int v);
    virtual logicWorker&  worker() = 0;
private:
    int  m_streamIndex{-1};
    int  m_serialCache;
};
#endif
