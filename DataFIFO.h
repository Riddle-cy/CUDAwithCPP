#pragma once

#include "cuda_runtime.h"
#include <cuda_profiler_api.h>

const int pixelNUM = 2304 * 1720;

typedef struct frame {
	unsigned int index;
	unsigned char* ptr;
}frameInfo;						//图像数据结构：图像帧号，像素数据


// FIFO队列,缓存图像裸数据
class FIFO {
private:
	int length;
	volatile bool exit;
	size_t emptyNUM, fullNUM;

public:
	frameInfo* bufferPtr;
	int front;
	int rear;
	int count;
	int fileFail;

	FIFO(int NUM) : length(NUM), front(0), rear(0) {
		//bufferPtr = new frameInfo[NUM];
		cudaHostAlloc((void**)&bufferPtr, NUM * sizeof(frameInfo), cudaHostAllocDefault);
		for (int i = 0; i < NUM; ++i) {
			cudaHostAlloc((void**)&bufferPtr[i].ptr, pixelNUM * sizeof(unsigned char), cudaHostAllocDefault);
			//bufferPtr[i].ptr = new unsigned char[pixelNUM];				//初始化内存，帧号
			bufferPtr[i].index = 0;
		}

		exit = false;
		count = 0;
		emptyNUM = fullNUM = 0;
		fileFail = 0;
	}

	~FIFO() {
		for (int i = 0; i < length; ++i)
			cudaFreeHost(bufferPtr[i].ptr);
			//delete[] bufferPtr[i].ptr;
		//delete[] bufferPtr;
		cudaFreeHost(bufferPtr);
	}

	int size() {
		return length;
	}

	int elemNUM() {
		return (rear - front + length) % length;
	}

	bool isEmpty() {
		return front == rear;
	}

	bool isFull() {
		return (rear + 1) % length == front;
	}

	bool isExit() {
		return exit;
	}

	void changeFlag() {
		exit = true;
	}

	void addEmpty() {
		++emptyNUM;
	}

	void addFull() {
		++fullNUM;
	}

	size_t getEmptyNUM() {
		return emptyNUM;
	}

	size_t getFullNUM() {
		return fullNUM;
	}
};


// FIFO队列,缓存图像裸数据
class L1_FIFO {
private:
	int length;
	bool exit;
	size_t emptyNUM, fullNUM;

public:
	frameInfo* bufferPtr;
	int front;
	int rear;
	int count;
	int fileFail;

	L1_FIFO(int NUM) : length(NUM), front(0), rear(0) {
		bufferPtr = new frameInfo[NUM];
		for (int i = 0; i < NUM; ++i) {
			bufferPtr[i].ptr = new unsigned char[pixelNUM];				//初始化内存，帧号
			bufferPtr[i].index = 0;
		}

		exit = false;
		count = 0;
		emptyNUM = fullNUM = 0;
		fileFail = 0;
	}

	~L1_FIFO() {
		for (int i = 0; i < length; ++i)
			delete[] bufferPtr[i].ptr;
		delete[] bufferPtr;
	}

	int size() {
		return length;
	}

	bool isEmpty() {
		return front == rear;
	}

	bool isFull() {
		return (rear + 1) % length == front;
	}

	bool isExit() {
		return exit;
	}

	void changeFlag() {
		exit = true;
	}

	void addEmpty() {
		++emptyNUM;
	}

	void addFull() {
		++fullNUM;
	}

	size_t getEmptyNUM() {
		return emptyNUM;
	}

	size_t getFullNUM() {
		return fullNUM;
	}
};
