#include<stdio.h>

#include "common.h"

extern cv::Mat I;

extern unsigned int imageSize;								//ͼ���С
extern unsigned short* code_out;						//GPU���ص�����
extern unsigned short* lenth_out;								
extern unsigned short* vecLen;									//��¼���볤��
extern unsigned short* vecCode;						//��¼����
extern unsigned char* h_bitres;

//�ṹ��洢���볤���Լ�����
typedef struct Code_Len {
	unsigned short code;
	unsigned short lenth;
}Code_Len;

bool InitCUDA();
void initBitTable(unsigned char ptr[], int n);
void FELICS(int& rows, int& cols, uchar* data, unsigned char* g_obitres, cudaStream_t* stream);