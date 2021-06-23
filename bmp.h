#pragma once

#include "grabber_cuda.h"

typedef struct {     //文件头信息 
	unsigned long bfSize;					//文件大小
	unsigned short bfReserved1;				//两个保留值，必须设置为0
	unsigned short bfReserved2;
	unsigned long bfOffBits;				//4字节，从头到位图数据的偏移量
}BmpFileHeader;

typedef struct {     //位图文件头
	unsigned long  biSize;					//信息头大小
	long   biWidth;							//以像素为单位说明图像宽度
	long   biHeight;						//以像素为单位说明图像高度
	unsigned short   biPlanes;				//颜色平面数，设置为1
	unsigned short   biBitCount;			//表示像素深度
	unsigned long  biCompression;			//4字节，压缩类型，设置为0，不压缩
	unsigned long  biSizeImage;				//4字节，说明位图数据大小，不压缩时设置为0
	long   biXPelsPerMeter;					//水平分辨率
	long   biYPelsPerMeter;					//垂直分辨率
	unsigned long   biClrUsed;				//说明位图使用的调色板中的颜色索引数，为0说明使用所有
	unsigned long   biClrImportant;			//说明对图像显示有重要影响的颜色索引数，为0说明都重要
}BmpInfoHeader;

typedef struct {      //调色板
	unsigned char rgbBlue;					//该颜色的蓝色分量  
	unsigned char rgbGreen;					//该颜色的绿色分量  
	unsigned char rgbRed;					//该颜色的红色分量  
	unsigned char rgbReserved;				//保留值，alpha
}RgbQuad;

typedef struct {      //图像数据
	int width;
	int height;
	int channels;							//通道数
	unsigned char* imageData;				//像素数据
}ClImage;

extern unsigned short fileType;			//文件类型，bmp文件为0x4D42

extern BmpFileHeader bmpFileHeader;		//文件头
extern BmpInfoHeader bmpInfoHeader;		//bmp头
extern RgbQuad* quad;					//调色板

extern int bmp_width;
extern int bmp_height;
extern int offset;
extern int picSize;

