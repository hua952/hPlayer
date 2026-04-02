#include "frameQue.h"
#include "strFun.h"

frameQue:: frameQue (const size_t capacity,  bool keepLast):m_que(capacity), m_keepLast(keepLast)
{
    auto& rQue = que();
    auto pF = rQue.nextWrite();
    while (pF) {
        pF->frame = av_frame_alloc();
        // 初始化 AVSubtitle 以及可疑字段，保证后续 avsubtitle_free 可安全调用
        std::memset(&pF->sub, 0, sizeof(pF->sub));
        pF->serial = 0;
        pF->pts = NAN;
        pF->duration = 0;
        pF->pos = 0;
        pF->width = pF->height = pF->format = 0;
        pF->uploaded = 0;
        pF->flip_v = 0;
        rQue.push();
        pF = rQue.nextWrite();
    }
    rQue.cPop();
    pF = rQue.nextWrite();
    if (pF) {
        pF->frame = av_frame_alloc();
        // 初始化 AVSubtitle 以及可疑字段，保证后续 avsubtitle_free 可安全调用
        std::memset(&pF->sub, 0, sizeof(pF->sub));
        pF->serial = 0;
        pF->pts = NAN;
        pF->duration = 0;
        pF->pos = 0;
        pF->width = pF->height = pF->format = 0;
        pF->uploaded = 0;
        pF->flip_v = 0;
        rQue.push();
        pF = rQue.nextWrite();
    }
    while(rQue.front()) {
        rQue.cPop();
    }
}

frameQue:: ~frameQue ()
{
    auto& rQue = que();
    auto pF = rQue.front();
    while (pF) {
        frameUnrefItem (*pF);
        av_frame_free(&pF->frame);
        rQue.pop();
        pF = rQue.front();
    }
}

frameQue::SPSCQue&  frameQue:: que ()
{
    return m_que;
}

bool frameQue:: keepLast ()
{
    return m_keepLast;
}

void  frameQue:: setKeepLast (bool v)
{
    m_keepLast = v;
}

packQue*  frameQue:: pktq ()
{
    return m_pktq;
}

void  frameQue:: setPktq (packQue* v)
{
    m_pktq = v;
}
cppFrame*   frameQue:: lastFrame()
{
    return que().front();
}
cppFrame*   frameQue:: curFrame()
{
    if (haveLastFrame())[[likely]] {
        return que().nextFront();
    } else {
        return que().front();
    }
}

cppFrame*   frameQue:: nextFrame()
{
    if (haveLastFrame())[[likely]] {
        return que().nextNextFront();
    } else {
        return que().nextFront();
    }
}

cppFrame*   frameQue:: nextWrite()
{
    return que().nextWrite();
}

void   frameQue:: popFrame()
{
    do {
        auto& rQue = que();
        auto pF = rQue.front();
        if (!pF) [[unlikely]] {
            break;
        }
        if (keepLast() && !haveLastFrame())  [[unlikely]] {
            setHaveLastFrame (true);
            break;
        }
        frameUnrefItem (*pF);
        rQue.pop();
    } while (0);
}

void  frameQue:: push()
{
    return que().push();
}

size_t   frameQue:: size()
{
    return que().size() - m_haveLastFrame;
}

void  frameQue:: frameUnrefItem(cppFrame& rF)
{
    // av_frame_unref(rF.frame);
    // avsubtitle_free(&rF.sub);
// 先对 AVFrame 做解引用（如果非空）
if (rF.frame)
    av_frame_unref(rF.frame);
// AVSubtitle 在分配时已用 memset 初始化为 0，若没有 rects 则 avsubtitle_free 是安全的
if (rF.sub.rects || rF.sub.num_rects)
    avsubtitle_free(&rF.sub);
}

bool   frameQue:: haveLastFrame()
{
    return m_haveLastFrame;
}

void   frameQue:: setHaveLastFrame (bool v)
{
    m_haveLastFrame = v? 1: 0;
}

