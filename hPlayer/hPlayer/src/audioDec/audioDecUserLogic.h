#include "logicWorker.h"

extern "C"
{
    #include "ffplayCom.h"
}

class audioDec;
class audioDecUserLogic: public IUserLogicWorker
{
public:
    enum audioLogicState
    {
        audioLogicState_readNotInit,
        audioLogicState_thisNeetInit,
        audioLogicState_willExit,
        audioLogicState_ok,
    };
    audioDecUserLogic (audioDec& rServer);
	int onLoopBegin() override;
	int onLoopEnd() override;
	int onLoopFrame() override;
    audioDec& getServer(){return m_raudioDec;}
    audioLogicState state ();
    void  setState(audioLogicState st);
    int  initThis();
    void  clean();
private:
    audioDec& m_raudioDec;
    AVFrame* m_frame {nullptr};
    audioLogicState  m_state{audioLogicState_readNotInit};
};
