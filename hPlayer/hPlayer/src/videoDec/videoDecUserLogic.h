#include "logicWorker.h"
#include "baseUserLogic.h"

extern "C"
{
    #include "ffplayCom.h"
}

class videoDec;
class videoDecUserLogic: public IUserLogicWorker, public baseUserStreamLogic
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
    int configure_video_filters(AVFilterGraph *graph, VideoState *is, const char *vfilters, AVFrame *frame);
    videoDecUserLogic (videoDec& rServer);
	int onLoopBegin() override;
	int onLoopEnd() override;
	int onLoopFrame() override;
    videoDec& getServer(){return m_rvideoDec;}
    logicWorker&  worker() override;
    videoDecLogicState  state ();
    void                setState(videoDecLogicState  st);
    int  initThis();
    void  clean();
    AVFilterContext*  in_video_filter ();
    void  setIn_video_filter (AVFilterContext* v);
    AVFilterContext*  out_video_filter ();
    void  setOut_video_filter (AVFilterContext* v);
    int  initVideo(int stream_index);
private:
    AVFilterContext*  m_out_video_filter{nullptr};
    AVFilterContext*  m_in_video_filter{nullptr};
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
