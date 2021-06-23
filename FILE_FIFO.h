#pragma once

#include <stdio.h>
#include <stdlib.h>

class myFileBuffer {
public:
	myFileBuffer(int len) : front(0), rear(0), length(len) {
		filePtr = (FILE**)malloc(sizeof(FILE*) * len);
		exit = false;
		emptyNUM = fullNUM = 0;
		mod = length - 1;
	}

	~myFileBuffer() {
		free((void*)filePtr);
	}

	bool isEmpty() {
		return front == rear;
	}

	bool isFull() {
		return (rear + 1) % length == front;
	}

	int size() {
		return this->length;
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

	int getMOD() {
		return mod;
	}

	FILE** filePtr;
	int front;
	int rear;

private:
	int length;
	int mod;
	volatile bool exit;
	size_t emptyNUM, fullNUM;
};
