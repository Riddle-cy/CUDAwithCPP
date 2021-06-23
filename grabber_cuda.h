#pragma once

#include <EGrabber.h>
#include "myWatch.h"

class MyGrabber : public Euresys::EGrabber<Euresys::CallbackSingleThread> {                  //封装自己的捕获器类，多线程回调
public:
    MyGrabber(Euresys::EGenTL& gentl);
    ~MyGrabber();
    size_t picNUM();
    int lostFrame();
    myWatch watch;
private:
    virtual void onNewBufferEvent(const Euresys::NewBufferData& data);
    bool Exit_Flag;
    int lostframe;
    volatile size_t count;
};

extern Euresys::Buffer* currentBuffer;
extern MyGrabber* grabber;