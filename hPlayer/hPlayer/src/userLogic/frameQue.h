#ifndef _frameQue_h__
#define _frameQue_h__
#include <memory>
#include "comFun.h"

extern "C"
{
    #include "ffplayCom.h"
}
// #include "cppCom.h"

#include "pSPSCQueue.h"

class cppFrame {
public:
    AVFrame *frame;
    AVSubtitle sub;
    int serial;
    double pts;           /* presentation timestamp for the frame */
    double duration;      /* estimated duration of the frame */
    int64_t pos;          /* byte position of the frame in the input file */
    int width;
    int height;
    int format;
    AVRational sar;
    int uploaded;
    int flip_v;
};


class packQue;

class frameQue
{
public:
    using SPSCQue = SPSCQueue2<cppFrame>;
    frameQue (const size_t capacity, bool keepLast );
    ~frameQue ();
    void frameUnrefItem(cppFrame& rF);
    bool keepLast ();
    void  setKeepLast (bool v);
    packQue*  pktq ();
    void  setPktq (packQue* v);
    cppFrame*  lastFrame();
    cppFrame*  curFrame();
    cppFrame*  nextFrame();
    cppFrame*  nextWrite();
    void push();
    virtual  void  popFrame();
    SPSCQue&  que ();
    size_t  size();
    bool  haveLastFrame();
    void  setHaveLastFrame (bool v);
private:
    SPSCQue   m_que;
    packQue*  m_pktq;
    bool      m_keepLast {false};
    ubyte     m_haveLastFrame {0};
};
#endif
