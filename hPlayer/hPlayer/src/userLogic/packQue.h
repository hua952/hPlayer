#ifndef _packQue_h__
#define _packQue_h__

extern "C"
{
    #include "ffplayCom.h"
}

#include "pSPSCQueue.h"
#include <atomic>
#include <memory>
#include <libavcodec/avcodec.h>
struct packNode
{
    packNode (AVPacket *pkt, int serial);
    packNode ();
    ~packNode ();
    AVPacket *m_pkt;
    int m_serial;
};
/*
// 方案 A：函数指针作为删除器（最简单）
struct AVPacketDeleter {
    void operator()(AVPacket* pkt) const {
        av_packet_free(&pkt);  // 注意：av_packet_free 需要 AVPacket**
    }
};
*/
// 类型别名
// using AVPacketPtr = std::unique_ptr<AVPacket, AVPacketDeleter>;

class packQue :public SPSCQueue2<packNode>
{
public:
    explicit packQue (const size_t capacity);
    void start ();
    int allPackSize ();
    void  allPackSizeAdd(int nAdd);
    int64_t allPackDuration ();
    void allPackDurationAdd (int64_t  nAdd);
    /*
    int abort_request ();
    void setAbort_request (int nAbort);
    */
    int serial ();
    bool  procLastUnpushPack();
    void  pushPack(AVPacket *pkt);
    void  pushImportant(AVPacket* ppkt);
    void cleanForSeek();
private:
    void clean();
    packNode m_lastUnpushpkt;
    std::atomic<int64_t> m_allPackDuration{ 0 };
    std::atomic<int> m_allPackSize{ 0 };
    // std::atomic<int> m_abort_request{0};
    std::atomic<int> m_serial{0};
};
#endif
