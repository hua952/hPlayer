#include "logicWorker.h"

class decoTh;
class decoThUserLogic: public IUserLogicWorker
{
public:
    decoThUserLogic (decoTh& rServer);
	int onLoopBegin() override;
	int onLoopEnd() override;
	int onLoopFrame() override;
    decoTh& getServer(){return m_rdecoTh;}
private:
    decoTh& m_rdecoTh;
};
