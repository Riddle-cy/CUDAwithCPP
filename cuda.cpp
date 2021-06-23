#include<iostream>
#include <fstream>


#include <cuda_runtime.h>
//#include <cuda_gl_interop.h>
#include <cuda_profiler_api.h>

#include"cuda.h"
#include"grabber_cuda.h"

//≥ı ºªØCUDA
bool InitCUDA() {
	int count;

	cudaGetDeviceCount(&count);
	if (count == 0) {
		fprintf(stderr, "There is no device.\n");
		return false;
	}

	int i;
	for (i = 0; i < count; i++) {
		cudaDeviceProp prop;
		if (cudaGetDeviceProperties(&prop, i) == cudaSuccess) {
			if (prop.major >= 1) {
				break;
			}
		}
	}

	if (i == count) {
		fprintf(stderr, "There is no device supporting CUDA.\n");
		return false;
	}

	cudaSetDevice(i);
	return true;
}

void initBitTable(unsigned char ptr[], int n) {
	if (n < 0) {
		printf("Table size error!\n");
		return;
	}
	for (int i = 0;i < n;++i) {
		if (i < 2)
			ptr[i] = 1;
		else if (i >= 2 && i < 4)
			ptr[i] = 2;
		else if (i >= 4 && i < 8)
			ptr[i] = 3;
		else if (i >= 8 && i < 16)
			ptr[i] = 4;
		else if (i >= 16 && i < 32)
			ptr[i] = 5;
		else if (i >= 32 && i < 64)
			ptr[i] = 6;
		else if (i >= 64 && i < 128)
			ptr[i] = 7;
		else if (i >= 128 && i < 256)
			ptr[i] = 8;
		else if (i >= 256 && i < 512)
			ptr[i] = 9;
		else if (i >= 512 && i < 1024)
			ptr[i] = 10;
		else if (i >= 1024 && i < 2048)
			ptr[i] = 11;
		else if (i >= 2048 && i < 4096)
			ptr[i] = 12;
		else if (i >= 4096 && i < 8192)
			ptr[i] = 13;
		else if (i >= 8192 && i < 16384)
			ptr[i] = 14;
		else if (i >= 16384 && i < 32768)
			ptr[i] = 15;
		else if (i >= 32768 && i < 65536)
			ptr[i] = 16;
	}
}
