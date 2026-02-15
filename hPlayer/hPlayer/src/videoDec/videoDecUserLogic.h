#include "logicWorker.h"

class videoDec;
class videoDecUserLogic: public IUserLogicWorker
{
public:
    videoDecUserLogic (videoDec& rServer);
	int onLoopBegin() override;
	int onLoopEnd() override;
	int onLoopFrame() override;
    videoDec& getServer(){return m_rvideoDec;}
private:
    videoDec& m_rvideoDec;
};
