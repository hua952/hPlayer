#ifndef _cppClock_h__
#define _cppClock_h__
#include <memory>

class packQue;
class cppClock
{
public:
    cppClock (packQue& q);
    ~cppClock ();
    void setClock(double pts, int serial);
    void setClockAt(double pts, int serial, double time);
    double getClock();
    int  serial ();
    void  setSerial (int v);
    double  pts ();
    void    setPts(double v);
    double  lastUpdated ();
    void    setLastUpdated (double v);
    int  paused ();
    void  setPaused (int v);
private:
    double  m_lastUpdated;
    double pts_drift;     /* clock base minus time at which we updated the clock */
    double last_updated;
    double speed;
    // double  m_pts;
    // int  m_serial;

    std::atomic<double> m_aPts{ 0 };
    std::atomic<int> m_aSerial{ 0 };
    // int *queue_serial;    /* pointer to the current packet queue serial, used for obsolete clock detection */

    int  m_paused;
    packQue&  m_packQ;
};
#endif
