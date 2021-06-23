#pragma once

#include<windows.h>

// 计时器，毫秒级
class myWatch {
public:
	myWatch() {
		QueryPerformanceFrequency(&freq);
	}

	~myWatch() {

	}

	void Start() {
		QueryPerformanceCounter(&start);
	}

	void End() {
		QueryPerformanceCounter(&end);
		time = ((double)end.QuadPart - (double)start.QuadPart) / (double)freq.QuadPart;
	}

	double getTime() {
		return time;
	}

private:
	LARGE_INTEGER start;		//开始时间
	LARGE_INTEGER end;			//结束时间
	LARGE_INTEGER freq;			//CPU频率
	double time;
};