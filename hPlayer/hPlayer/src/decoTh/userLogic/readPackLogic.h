#ifndef _readPackLogic_h__
#define _readPackLogic_h__
#include <memory>
#include "ffplayCom.h"

class decoTh;
class readPackLogic
{
public:
    enum readState
    {
        readState_mainNotInit,
        readState_thisNeetInit,
        readState_ok,
        readState_playEnd,
        readState_waiteSubExit,
        readState_willExit,
    };
    readPackLogic (decoTh& rDecoTh);
    ~readPackLogic ();
    int initThis();

    int onLoopBegin();
    int onLoopFrame();
    int onLoopEnd();
    
    void sendExitNtfToSub();
    void tryClean();
    readState  state ();
    void  setState(readState  st);
    decoTh&    getDecoTh();
    void       clean();
    bool  videoHaveClean ();
    void  setVideoHaveClean (bool v);
    bool  audioHaveClean ();
    void  setAudioHaveClean (bool v);
    bool  subTitleDecHaveClean ();
    void  setSubTitleDecHaveClean (bool v);
    void  sendEmptyAudioPack();
    void  sendEmptyVideoPack();
private:
    bool  m_subTitleDecHaveClean = true;
    bool  m_audioHaveClean = true;
    bool  m_videoHaveClean = true;
    decoTh&    m_rDecoTh;

    AVFormatContext* m_ic = nullptr;
    SDL_mutex* m_wait_mutex= nullptr;
    AVPacket* m_pkt = nullptr;
    int        m_ret = 0;
    readState  m_readState;
};
#endif
