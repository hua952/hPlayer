#ifndef _videoDecLogic_h__
#define _videoDecLogic_h__
#include <memory>

#include "ffplayCom.h"

class videoDec;
class videoDecLogic
{
public:
    enum videoDecLogicState
    {
        videoDecLogicState_readNotInit,
        videoDecLogicState_thisNeetInit,
        videoDecLogicState_endDec,
        videoDecLogicState_willExit,
        videoDecLogicState_ok
    };
    videoDecLogic (videoDec& rVideo);
    ~videoDecLogic ();

    int onLoopBegin();
    int onLoopFrame();
    int onLoopEnd();
    videoDecLogicState  state ();
    void                setState(videoDecLogicState  st);
    int  initThis();
    void  clean();

    videoDec& getVideoDec();
private:
    videoDec& m_rVideoDec;
    AVFrame* m_frame {nullptr};
    AVFilterGraph* m_graph {nullptr};
    AVFilterContext* m_filt_out{nullptr};
    AVFilterContext* m_filt_in{nullptr};
    int m_last_w = 0;
    int m_last_h = 0;
    enum AVPixelFormat m_last_format = (enum AVPixelFormat)-2;
    int m_last_serial = -1;
    int m_last_vfilter_idx = 0;
    AVRational m_frame_rate;
    videoDecLogicState  m_state{videoDecLogicState_readNotInit};
};
#endif
