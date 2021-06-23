#include<stdio.h>

#include "common.h"

extern cv::Mat I;

extern unsigned int imageSize;								//图像大小
extern unsigned short* code_out;						//GPU传回的数据
extern unsigned short* lenth_out;								
extern unsigned short* vecLen;									//记录编码长度
extern unsigned short* vecCode;						//记录码流
extern unsigned char* h_bitres;

//结构体存储编码长度以及码流
typedef struct Code_Len {
	unsigned short code;
	unsigned short lenth;
}Code_Len;

bool InitCUDA();
void initBitTable(unsigned char ptr[], int n);
void FELICS(int& rows, int& cols, uchar* data, unsigned char* g_obitres, cudaStream_t* stream);