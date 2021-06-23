#pragma once

#include "grabber_cuda.h"

typedef struct {     //�ļ�ͷ��Ϣ 
	unsigned long bfSize;					//�ļ���С
	unsigned short bfReserved1;				//��������ֵ����������Ϊ0
	unsigned short bfReserved2;
	unsigned long bfOffBits;				//4�ֽڣ���ͷ��λͼ���ݵ�ƫ����
}BmpFileHeader;

typedef struct {     //λͼ�ļ�ͷ
	unsigned long  biSize;					//��Ϣͷ��С
	long   biWidth;							//������Ϊ��λ˵��ͼ����
	long   biHeight;						//������Ϊ��λ˵��ͼ��߶�
	unsigned short   biPlanes;				//��ɫƽ����������Ϊ1
	unsigned short   biBitCount;			//��ʾ�������
	unsigned long  biCompression;			//4�ֽڣ�ѹ�����ͣ�����Ϊ0����ѹ��
	unsigned long  biSizeImage;				//4�ֽڣ�˵��λͼ���ݴ�С����ѹ��ʱ����Ϊ0
	long   biXPelsPerMeter;					//ˮƽ�ֱ���
	long   biYPelsPerMeter;					//��ֱ�ֱ���
	unsigned long   biClrUsed;				//˵��λͼʹ�õĵ�ɫ���е���ɫ��������Ϊ0˵��ʹ������
	unsigned long   biClrImportant;			//˵����ͼ����ʾ����ҪӰ�����ɫ��������Ϊ0˵������Ҫ
}BmpInfoHeader;

typedef struct {      //��ɫ��
	unsigned char rgbBlue;					//����ɫ����ɫ����  
	unsigned char rgbGreen;					//����ɫ����ɫ����  
	unsigned char rgbRed;					//����ɫ�ĺ�ɫ����  
	unsigned char rgbReserved;				//����ֵ��alpha
}RgbQuad;

typedef struct {      //ͼ������
	int width;
	int height;
	int channels;							//ͨ����
	unsigned char* imageData;				//��������
}ClImage;

extern unsigned short fileType;			//�ļ����ͣ�bmp�ļ�Ϊ0x4D42

extern BmpFileHeader bmpFileHeader;		//�ļ�ͷ
extern BmpInfoHeader bmpInfoHeader;		//bmpͷ
extern RgbQuad* quad;					//��ɫ��

extern int bmp_width;
extern int bmp_height;
extern int offset;
extern int picSize;

