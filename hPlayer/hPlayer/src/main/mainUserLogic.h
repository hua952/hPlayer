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
    bool  forceRefresh ();
    void  setForceRefresh (bool v);
private:
    bool  m_forceRefresh{false};
    main& m_rmain;
    mainState   m_mainState = mainState_noWindow;
private:
    int ffplay_event_loop();
    void toggle_audio_display();
    void refresh_loop_wait_event(VideoState *is, SDL_Event *event);
    void cpp_video_refresh(void *opaque, double *remaining_time);
};
