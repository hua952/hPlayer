#include "logicWorker.h"

extern "C"
{
    #include "ffplayCom.h"
}

class subtitleqDec;
class subtitleqDecUserLogic: public IUserLogicWorker
{
public:
    enum subtitleqLogicState
    {
        subtitleqLogicState_readNotInit,
        subtitleqLogicState_thisNeetInit,
        subtitleqLogicState_waitExit,
        subtitleqLogicState_willExit,
        subtitleqLogicState_ok
    };
    subtitleqDecUserLogic (subtitleqDec& rServer);
	int onLoopBegin() override;
	int onLoopEnd() override;
	int onLoopFrame() override;
    subtitleqDec& getServer(){return m_rsubtitleqDec;}
    subtitleqLogicState  state ();
    void setState (subtitleqLogicState st);
    int  initThis();
    void  clean();
private:
    subtitleqLogicState  m_state{subtitleqLogicState_readNotInit};
    subtitleqDec& m_rsubtitleqDec;
};
