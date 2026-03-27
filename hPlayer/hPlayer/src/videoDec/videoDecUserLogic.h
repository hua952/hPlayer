#include "logicWorker.h"


extern "C"
{
    #include "ffplayCom.h"
}

class videoDec;
class videoDecUserLogic: public IUserLogicWorker
{
public:
    enum videoDecLogicState
    {
        videoDecLogicState_readNotInit,
        videoDecLogicState_thisNeetInit,
        videoDecLogicState_waitExit,
        videoDecLogicState_willExit,
        videoDecLogicState_ok
    };
    videoDecUserLogic (videoDec& rServer);
	int onLoopBegin() override;
	int onLoopEnd() override;
	int onLoopFrame() override;
    videoDec& getServer(){return m_rvideoDec;}

    videoDecLogicState  state ();
    void                setState(videoDecLogicState  st);
    int  initThis();
    void  clean();
private:
    AVFrame* m_frame {nullptr};
    AVFilterGraph* m_graph {nullptr};
    AVRational m_frame_rate;
    AVFilterContext* m_filt_out{nullptr};
    AVFilterContext* m_filt_in{nullptr};
    int m_last_w = 0;
    int m_last_h = 0;
    enum AVPixelFormat m_last_format = (enum AVPixelFormat)-2;
    int m_last_serial = -1;
    int m_last_vfilter_idx = 0;
    videoDec& m_rvideoDec;
    videoDecLogicState  m_state{videoDecLogicState_readNotInit};
};
