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

    int audio_decode_frame(VideoState *is);
    logicWorker&  worker() override;
    int  initAudio(int stream_index);

    struct AudioParams& audioTgt();
    uint8_t*  audio_buf ();
    void  setAudio_buf (uint8_t* v);
    void do_sdl_audio_callback(Uint8 *stream, int len);
private:
    int audio_diff_avg_count{0};
    int audio_hw_buf_size{0};
    int audio_buf_index{0};
    unsigned int audio_buf_size{0};
    unsigned int audio_buf1_size{0};
    uint8_t *audio_buf1{nullptr};
    uint8_t*  m_audio_buf{nullptr};
    struct AudioParams m_audio_tgt;
    struct AudioParams audio_src;
    struct AudioParams audio_filter_src;
    struct SwrContext* swr_ctx {nullptr};
    AVFilterGraph *agraph {nullptr};              // audio filter graph
    AVFilterContext *in_audio_filter {nullptr};   // the first filter in the audio chain
    AVFilterContext *out_audio_filter {nullptr};  // the last filter in the audio chain
    bufQue  m_bufQue;
    audioDec& m_raudioDec;
    AVFrame* m_frame {nullptr};
    audioLogicState  m_state{audioLogicState_readNotInit};
private:
    int synchronize_audio(VideoState *is, int nb_samples);
    int configure_audio_filters(VideoState *is, const char *afilters, int force_output_format);
};
