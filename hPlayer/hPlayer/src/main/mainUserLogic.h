#include "logicWorker.h"
#include "baseUserLogic.h"
extern "C"
{
    #include "ffplayCom.h"
}
class main;
class mainUserLogic: public IUserLogicWorker, public baseUserLogic
{
public:
    enum mainState 
    {
        mainState_noWindow,
        mainState_haveWindow,
        mainState_willExit,
    };
    mainUserLogic (main& rServer);
	int onLoopBegin() override;
	int onLoopEnd() override;
	int onLoopFrame() override;
    main& getServer(){return m_rmain;}
    mainState state();
    void  setState(mainState st);
    
    int initThis();
    void       clean();
private:
    int ffplay_event_loop(VideoState *cur_stream);
    main& m_rmain;
    mainState   m_mainState = mainState_noWindow;
};
