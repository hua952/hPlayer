#include <deque>

#include "baseUserLogic.h"
extern "C"
{
    #include "ffplayCom.h"
}
#include "cppCom.h"

#include "logicWorker.h"

class audioDec;
class audioDecUserLogic: public IUserLogicWorker, public baseUserLogic
{
public:
    enum audioLogicState
    {
        audioLogicState_readNotInit,
        audioLogicState_thisNeetInit,
        audioLogicState_willExit,
        audioLogicState_ok,
    };
    using bufQue = std::deque<cppFrame>;
    audioDecUserLogic (audioDec& rServer);
	int onLoopBegin() override;
	int onLoopEnd() override;
	int onLoopFrame() override;
    audioDec& getServer(){return m_raudioDec;}
    audioLogicState state ();
    void  setState(audioLogicState st);
    int  initThis();
    void  clean();
    bufQue&  getBufQue ();
private:
    bufQue  m_bufQue;
    audioDec& m_raudioDec;
    AVFrame* m_frame {nullptr};
    audioLogicState  m_state{audioLogicState_readNotInit};
};
