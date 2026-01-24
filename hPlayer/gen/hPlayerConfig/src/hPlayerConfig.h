#ifndef _hPlayerConfig__h_
#define _hPlayerConfig__h_

#include "comFun.h"
#include <memory>
class hPlayerConfig
{
public:
	hPlayerConfig ();
	int  procCmdArgS (int nArg, char** argS);
	int  dumpConfig (const char* szFile);
	int  loadConfig (const char* szFile);
		  const char*  modelS ();
  void  setModelS (const char* v);
  const char*  playFile ();
  void  setPlayFile (const char* v);
private:
  std::unique_ptr <char[]>  m_modelS;
  std::unique_ptr <char[]>  m_playFile;
};
#endif
