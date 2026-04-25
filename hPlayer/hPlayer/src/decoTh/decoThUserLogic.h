#include "logicWorker.h"

extern "C"
{
    #include "ffplayCom.h"
}


class decoTh;
class decoThUserLogic: public IUserLogicWorker
{
public:
    enum readState
    {
        readState_mainNotInit,
        readState_thisNeetInit,
        readState_ok,
        readState_waiteSubExit,
        readState_willExit,
    };
    decoThUserLogic (decoTh& rServer);
	int onLoopBegin() override;
	int onLoopEnd() override;
	int onLoopFrame() override;
    decoTh& getServer(){return m_rdecoTh;}

    int initThis();
    readState  state ();
    void  setState(readState  st);
    void       clean();
    /*
    void sendExitNtfToSub();
    void    sendEmptySubtitleqPack();
    void  sendEmptyAudioPack();
    */
private:
    decoTh& m_rdecoTh;
    readState  m_readState {readState_mainNotInit};
    AVFormatContext* m_ic = nullptr;
    // SDL_mutex* m_wait_mutex= nullptr;
    AVPacket* m_pkt = nullptr;
    int        m_ret = 0;
};
