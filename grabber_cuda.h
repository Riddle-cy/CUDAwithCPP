#pragma once

#include <EGrabber.h>
#include "myWatch.h"

class MyGrabber : public Euresys::EGrabber<Euresys::CallbackSingleThread> {                  //��װ�Լ��Ĳ������࣬���̻߳ص�
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