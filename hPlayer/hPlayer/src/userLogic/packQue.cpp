#include "packQue.h"
#include "strFun.h"
#include "myAssert.h"

packNode ::packNode (AVPacket *pkt, int serial):m_pkt(pkt), m_serial(serial)
{
}

packNode ::packNode ():m_pkt(nullptr), m_serial(-1)
{
}


packNode ::~packNode ()
{
    av_packet_free(&m_pkt);
}

packQue:: packQue (const size_t capacity):SPSCQueue2(capacity)
{
}

int  packQue:: allPackSize ()
{
    return m_allPackSize.load(std::memory_order_relaxed);
}

void   packQue:: allPackSizeAdd(int nAdd)
{
    m_allPackSize.fetch_add(nAdd, std::memory_order_relaxed);
}

int64_t  packQue:: allPackDuration ()
{
    return m_allPackDuration.load(std::memory_order_relaxed);
}

void packQue:: allPackDurationAdd (int64_t  nAdd)
{
    m_allPackDuration.fetch_add(nAdd, std::memory_order_relaxed);
}
/*
int  packQue:: abort_request ()
{
    return m_abort_request.load(std::memory_order_relaxed);
}

void  packQue:: setAbort_request (int nAbort)
{
    m_abort_request.store(nAbort, std::memory_order_relaxed);
}
*/
int  packQue:: serial ()
{
    return m_serial.load(std::memory_order_relaxed);
}

void  packQue:: start ()
{
    // m_abort_request.store(0, std::memory_order_relaxed);
    m_serial.fetch_add(1, std::memory_order_relaxed);
}


bool   packQue:: procLastUnpushPack()
{
    bool   nRet = true;
    if (m_lastUnpushpkt.m_pkt) {
        nRet = try_push(m_lastUnpushpkt);
        if (nRet) {
            m_lastUnpushpkt.m_pkt = nullptr;
        } 
    }
    
    return nRet;
}

void   packQue:: pushPack(AVPacket *pkt)
{
    do {
        myAssert(!m_lastUnpushpkt.m_pkt);
        m_lastUnpushpkt.m_pkt = av_packet_alloc();
        if (!m_lastUnpushpkt.m_pkt) {
            av_packet_unref(pkt);
            break;
        }
        av_packet_move_ref(m_lastUnpushpkt.m_pkt, pkt);
        m_lastUnpushpkt.m_serial = serial ();
        if (try_push(m_lastUnpushpkt)) {
            m_lastUnpushpkt.m_pkt = nullptr;
        } else {
            myAssert(m_lastUnpushpkt.m_pkt);
        }
    } while (0);
}

void   packQue:: pushImportant(AVPacket* pkt)
{
    do {
        auto bC = procLastUnpushPack();
        if (!bC) {
            av_packet_free(&m_lastUnpushpkt.m_pkt);
        }
        pushPack(pkt);
    } while (0);
}

void  packQue:: clean()
{
    while (front()) {
      pop();
    }
    av_packet_free(&m_lastUnpushpkt.m_pkt);
}

void  packQue:: cleanForSeek()
{
    clean ();
    m_allPackDuration.store(0, std::memory_order_relaxed);
    m_allPackSize.store(0, std::memory_order_relaxed);
    m_serial.fetch_add(1, std::memory_order_relaxed);
}

