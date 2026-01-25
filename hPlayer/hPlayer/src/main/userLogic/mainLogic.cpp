#include "mainLogic.h"
#include "strFun.h"
#include "gLog.h"
#include "hPlayerConfig.h"
#include "tSingleton.h"
#include "playerDataRpc.h"
#include "main.h"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_vulkan.h"
#include "globalData.h"

mainLogic:: mainLogic (main& rMain):m_rMain(rMain)
{
}

mainLogic:: ~mainLogic ()
{
}

VulkanRenderer&  mainLogic:: render ()
{
    return m_render;
}

imGuiMgr&  mainLogic:: imguiMgr ()
{
    return m_imguiMgr;
}

int  mainLogic:: onLoopBegin()
{
    int  nRet = 0;
    do {
        const char* filename = nullptr;
        
        m_render.addDrawCallback(&m_imguiMgr);
        
        auto& rUserConfig = tSingleton<hPlayerConfig>::single ();
        std::string strFileName;
        auto playFile = rUserConfig.playFile ();
        if (playFile) {
            if (strlen(playFile)) {
                strFileName = playFile;
            }
        }
        if (!strFileName.empty()) {
            playURLAskMsg  ask;
            auto pAsk = ask.pack();
            strNCpy(pAsk->m_url, sizeof(pAsk->m_url), strFileName.c_str());
            m_rMain.sendMsg(ask);
        }
    } while (0);
    return nRet;
}

int  mainLogic:: onLoopFrame()
{
    int  nRet = 0;
    do {
        auto& que = getDecodeRenderQueue();
        auto pFrame = que.front ();
        if (mainState_noWindow == m_mainState) [[unlikely]]{
            if (!pFrame) {
                break;
            }
            m_window = SDL_CreateWindow("Vulkan Player", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, pFrame->m_width, pFrame->m_height, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE/* | SDL_WINDOW_HIDDEN*/);
            if (!m_window) {
                gError("SDL_CreateWindow failed " << SDL_GetError());
                nRet = procPacketFunRetType_exitAfterLoop;
                // SDL_Quit();
                break;
            }
            try {
                m_render.setCurVideoWidth (pFrame->m_width);
                m_render.setCurVideoHeight(pFrame->m_height);
                m_render.init(m_window);
                gInfo("Vulkan initialized successfully ycbcrFeatureEnabled = " << (m_render.ycbcrFeatureEnabled()? "true" : "false"));
            }
            catch (const std::exception& e) {
                gError("Vulkan init failed: " << e.what());
                SDL_Delay(5000);
                nRet = procPacketFunRetType_exitAfterLoop;
                break;
            }
            m_mainState = mainState_haveWindow;
        }
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            ImGui_ImplSDL2_ProcessEvent(&ev);
            if (ev.type == SDL_QUIT || (ev.type == SDL_WINDOWEVENT && ev.window.event == SDL_WINDOWEVENT_CLOSE)) {
                nRet = procPacketFunRetType_exitAfterLoop;
                break;
            }
            if (ev.type == SDL_WINDOWEVENT) {
                if (ev.window.event == SDL_WINDOWEVENT_CLOSE) {
                    nRet = procPacketFunRetType_exitAfterLoop;
                    break;
                }
                if (ev.window.event == SDL_WINDOWEVENT_MINIMIZED) {
                    m_render.setState(VulkanRenderer::renderState_noSwapchain);
                }
                if (ev.window.event == SDL_WINDOWEVENT_RESTORED) {
                    m_render.setState(VulkanRenderer::renderState_OK);
                }
            }
        }
        if (procPacketFunRetType_exitAfterLoop & nRet) {
            break;
        }

        while(pFrame) {
            auto& rAsk = *pFrame;
            constexpr double SYNC_TOLERANCE = 0.05;
            double audio_time = mainClockSec();
            double video_time = rAsk.m_pts_seconds;
            double diff = video_time - audio_time;
            if (diff > SYNC_TOLERANCE) {
                break; /* 太快 */
            }
            if (diff > -SYNC_TOLERANCE) {
                bool ok = m_render.presentNV12(rAsk);
                if (!ok) {
                    gError("present failed");
                }
                que.pop();
                break; /* 正常播放一帧 */
            }  
            // popVideoFrame();
            que.pop();
            pFrame = que.front ();
            // pFrame = frontVideoFrame();  /* 丢弃过时的帧  */
            gInfo(" delete time out frame");
        }
    } while (0);
    return nRet;
}

int  mainLogic:: onLoopEnd()
{
    int  nRet = 0;
    do {
        m_render.cleanup();
        //m_imguiMgr.cleanup();
        SDL_DestroyWindow(m_window);
    } while (0);
    return nRet;
}

