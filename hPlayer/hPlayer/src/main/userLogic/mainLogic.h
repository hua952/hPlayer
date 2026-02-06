#ifndef _mainLogic_h__
#define _mainLogic_h__
#include <memory>
#include "VulkanRenderer.h"
#include "imGuiMgr.h"

class main;
struct VideoState;
class mainLogic
{
public:
    enum mainState 
    {
        mainState_noWindow,
        mainState_haveWindow,
        mainState_willExit,
    };
    mainLogic (main& rMain);
    ~mainLogic ();

    int onLoopBegin();
    int onLoopFrame();
    int onLoopEnd();
    VulkanRenderer&  render ();
    imGuiMgr&  imguiMgr ();

    // int m_curWinW = 680;
    // int m_curWinH = 680;
    mainState state();
    void  setState(mainState st);
    main&  getMain();
private:
    mainState   m_mainState = mainState_noWindow;
    main&            m_rMain;
    VulkanRenderer  m_render;
    imGuiMgr  m_imguiMgr{m_render};
    SDL_Window* m_window = nullptr;
    // VideoState* m_is = nullptr;
};
#endif
