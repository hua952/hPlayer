#ifndef _ffplayMain_h__
#define _ffplayMain_h__
#include <memory>

class mainLogic;
int ffplay_event_loop(VideoState *cur_stream, mainLogic& rMain);
#endif
