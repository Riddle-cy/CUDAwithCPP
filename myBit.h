#pragma once

#include<Windows.h>
#include <stdlib.h>

typedef unsigned char uint8;

typedef struct {
	uint8* start;		//指向buf头部指针
	uint8* p;			//当前指针位置
	uint8* end;			//buf尾部指针
	uint8 bits_left;		//可用bit数
}bs_t;

bs_t* bs_init(bs_t* b, uint8* buf, size_t size) {
	b->start = buf;
	b->p = buf;
	b->end = buf + size;
	b->bits_left = 8;
	return b;
}

bs_t* bs_new(uint8* buf, size_t size) {
	bs_t* b = (bs_t*)malloc(sizeof(bs_t));
	bs_init(b, buf, size);
	return b;  
}

void bs_free(bs_t* b) {
	free(b);
}

//判断是否写到了末尾
int bs_eof(bs_t* b) {
	if (b->p >= b->end)
		return 1;
	else
		return 0;
}

//写入1bit
void bs_write_1bit(bs_t* b, uint8 v) {
	--(b->bits_left);
	if (!bs_eof(b))
		(*(b->p)) |= ((v & 0x01) << b->bits_left);
	if (b->bits_left == 0) {	//写到字节末尾，移向下一个字，比特位初始化为8
		++b->p;
		b->bits_left = 8;
	}
}

/**
写入n个比特
@param b 比特流操作句柄
@param n 参数v所需的比特位个数
@param v 待写入的值
*/
void bs_write(bs_t* b, int n, unsigned short v) {
	for (int i = 0;i < n;++i) {
		bs_write_1bit(b, (unsigned char)(v >> (n - i - 1)) & 0x01);
	}
}

void bs_reset(uint8* buf, int n) {
	memset(buf, 0, n);
}