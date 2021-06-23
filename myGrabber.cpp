#include "grabber_cuda.h"

MyGrabber::MyGrabber(Euresys::EGenTL& gentl) :Euresys::EGrabberCallbackSingleThread(gentl) {
	count = 0;
	lostframe = 0;
	Exit_Flag = false;
	this->reallocBuffers(640);
	runScript("freerun.js");
}

MyGrabber::~MyGrabber() {
	shutdown();
}

size_t MyGrabber::picNUM() {
	return count;
}

int MyGrabber::lostFrame() {
	return lostframe;
}