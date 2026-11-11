#include <deque>

#include "baseUserLogic.h"
extern "C"
{
    #include "ffplayCom.h"
}
#include "cppCom.h"

#include "logicWorker.h"

class audioDec;
class audioDecUserLogic: public IUserLogicWorker, public baseUserStreamLogic
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

    logicWorker&  worker() override;
    int  initAudio(int stream_index);

private:
    AVFilterGraph *agraph {nullptr};              // audio filter graph
    AVFilterContext *in_audio_filter {nullptr};   // the first filter in the audio chain
    AVFilterContext *out_audio_filter {nullptr};  // the last filter in the audio chain
    bufQue  m_bufQue;
    audioDec& m_raudioDec;
    AVFrame* m_frame {nullptr};
    audioLogicState  m_state{audioLogicState_readNotInit};
private:
    int configure_audio_filters(VideoState *is, const char *afilters, int force_output_format);
};
