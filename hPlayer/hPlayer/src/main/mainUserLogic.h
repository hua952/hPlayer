#include "logicWorker.h"

class main;
class mainUserLogic: public IUserLogicWorker
{
public:
    mainUserLogic (main& rServer);
	int onLoopBegin() override;
	int onLoopEnd() override;
	int onLoopFrame() override;
    main& getServer(){return m_rmain;}
private:
    main& m_rmain;
};
