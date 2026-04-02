#include "cppClock.h"
#include "strFun.h"
#include "packQue.h"

extern "C"
{
#include "ffplayCom.h"
}


cppClock:: cppClock (packQue& q):m_packQ(q)
{
    speed = 1.0;
    m_paused = 0;

    setClock(NAN, -1);
}

cppClock:: ~cppClock ()
{
}

int  cppClock:: serial ()
{
    return m_aSerial.load(std::memory_order_relaxed);
}

void  cppClock:: setSerial (int v)
{
    m_aSerial.store(v, std::memory_order_relaxed);
}

void  cppClock:: setClock(double pts, int serial)
{

    double time = av_gettime_relative() / 1000000.0;
    setClockAt(pts, serial, time);
}

double   cppClock:: pts ()
{
    return m_aPts.load(std::memory_order_relaxed);
}

void     cppClock:: setPts(double v)
{
    m_aPts.store(v, std::memory_order_relaxed);
}

void  cppClock:: setClockAt(double pts, int serial, double time)
{
    // setPts(pts);
    last_updated = time;
    pts_drift = pts - time;
    // setSerial(serial);
    setPts(pts);
    m_aSerial.store(pts, std::memory_order_release);
}

double  cppClock:: getClock()
{
    auto aSer = m_aSerial.load(std::memory_order_acquire);
    if (m_packQ.serial() != aSer)
        return NAN;
    if (m_paused) {
        return pts();
    } else {
        double time = av_gettime_relative() / 1000000.0;
        return pts_drift + time - (time - m_lastUpdated) * (1.0 - speed);
    }
}

double   cppClock:: lastUpdated ()
{
    return m_lastUpdated;
}

void     cppClock:: setLastUpdated (double v)
{
    m_lastUpdated = v;
}

int  cppClock:: paused ()
{
    return m_paused;
}

void  cppClock:: setPaused (int v)
{
    m_paused = v;
}

