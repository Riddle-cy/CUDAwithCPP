#pragma once

#include<windows.h>

// ��ʱ�������뼶
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
	LARGE_INTEGER start;		//��ʼʱ��
	LARGE_INTEGER end;			//����ʱ��
	LARGE_INTEGER freq;			//CPUƵ��
	double time;
};