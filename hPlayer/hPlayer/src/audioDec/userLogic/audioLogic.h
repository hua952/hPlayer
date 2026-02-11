#ifndef _audioLogic_h__
#define _audioLogic_h__
#include <memory>

#include "ffplayCom.h"

class audioDec;
class audioLogic
{
public:
    enum audioLogicState
    {
        audioLogicState_readNotInit,
        audioLogicState_thisNeetInit,
        audioLogicState_willExit,
        audioLogicState_ok,
    };
    audioLogic (audioDec& rDec);
    ~audioLogic ();

    int onLoopBegin();
    int onLoopFrame();
    int onLoopEnd();
    audioLogicState state ();
    void  setState(audioLogicState st);
    int  initThis();
    void  clean();

    audioDec&        getAudioDec();
private:
    audioDec&        m_rAudioDec;
    audioLogicState  m_state{audioLogicState_readNotInit};
    AVFrame* m_frame {nullptr};
};
#endif
