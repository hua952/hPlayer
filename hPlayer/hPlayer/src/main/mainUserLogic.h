#include "logicWorker.h"

class main;
class mainUserLogic: public IUserLogicWorker
{
public:
    enum mainState 
    {
        mainState_noWindow,
        mainState_haveWindow,
        mainState_willExit,
    };
    mainUserLogic (main& rServer);
	int onLoopBegin() override;
	int onLoopEnd() override;
	int onLoopFrame() override;
    main& getServer(){return m_rmain;}

    mainState state();
    void  setState(mainState st);
    
    int initThis();
    void       clean();
private:
    main& m_rmain;
    mainState   m_mainState = mainState_noWindow;
};
