#include "logicWorker.h"

class subtitleqDec;
class subtitleqDecUserLogic: public IUserLogicWorker
{
public:
    subtitleqDecUserLogic (subtitleqDec& rServer);
	int onLoopBegin() override;
	int onLoopEnd() override;
	int onLoopFrame() override;
    subtitleqDec& getServer(){return m_rsubtitleqDec;}
private:
    subtitleqDec& m_rsubtitleqDec;
};
