#include "logicWorker.h"
#include "ffplayCom.h"

class audioDec;
class audioDecUserLogic: public IUserLogicWorker
{
public:
    audioDecUserLogic (audioDec& rServer);
    enum audioLogicState
    {
        audioLogicState_readNotInit,
        audioLogicState_thisNeetInit,
        audioLogicState_willExit,
        audioLogicState_ok,
    };

    audioLogicState state ();
    void  setState(audioLogicState st);
    int  initThis();
    void  clean();
    int onLoopBegin() override;
	int onLoopEnd() override;
	int onLoopFrame() override;
    audioDec& getServer(){return m_raudioDec;}
private:
    audioDec& m_raudioDec;
    AVFrame* m_frame {nullptr};
    audioLogicState  m_state{audioLogicState_readNotInit};
};
