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
    logicWorker&  worker() override;
    mainState state();
    void  setState(mainState st);
    
    int initThis();
    void       clean();
    bool  forceRefresh ();
    void  setForceRefresh (bool v);
    int  width ();
    void  setWidth (int v);
    int  height ();
    void  setHeight (int v);
    int  xleft ();
    void  setXleft (int v);
    int  ytop ();
    void  setYtop (int v);
    int  xpos ();
    void  setXpos (int v);
    int  step ();
    void  setStep (int v);
private:
    int  m_step{0};
    int  m_xpos{0};
    int  m_ytop{0};
    int  m_xleft{0};
    int  m_height{0};
    int  m_width{0};
    bool  m_forceRefresh{false};
    main& m_rmain;
    mainState   m_mainState = mainState_noWindow;
private:
    void toggle_pause(VideoState *is);
    void step_to_next_frame(VideoState *is);
    void cpp_video_image_display(VideoState *is);
    void video_display(VideoState *is);
    int video_open(VideoState *is);
    void video_audio_display(VideoState *s);
    int ffplay_event_loop();
    void toggle_audio_display();
    void refresh_loop_wait_event(VideoState *is, SDL_Event *event);
    void cpp_video_refresh(void *opaque, double *remaining_time);
};
