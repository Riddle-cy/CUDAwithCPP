#pragma once

#include<opencv2/opencv.hpp>

#include "DataFIFO.h"
#include"FILE_FIFO.h"
#include "myWatch.h"
#include "bmp.h"
#include "cuda_runtime.h"
#include <cuda_profiler_api.h>

#define blockSize 1024
#define pixelSize 2304*1720
#define gridSize (pixelSize+blockSize-1)/blockSize
#define TEST_NUM 10
#define CaptureNUM 10
#define CaptureTIME 500    //s

#define L1_NUM 512
#define L2_NUM 512

#define CUDACHECK(cmd) do {                         \
  cudaError_t e = cmd;                              \
  if( e != cudaSuccess ) {                          \
    printf("Failed: Cuda error %s:%d '%s'\n",       \
        __FILE__,__LINE__,cudaGetErrorString(e));   \
    exit(1);                                        \
  }                                                 \
} while(0)