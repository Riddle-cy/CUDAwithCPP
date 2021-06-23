
#include <ctime>

#include "device_launch_parameters.h"
#include "device_functions.h"
#include "cuda.h"
#include "common.h"
#include "cuda_runtime.h"
#include <cuda_profiler_api.h>

//编码时由于先存储结果的int型，获取对应二进制的长度,__clz()为CUDA内建函数
//返回一个整数的二进制长度，0时返回1
__device__ 
int countNum(unsigned int x) {
	//返回x对应的二进制值的位数,为0时返回1，__clz()为cuda库函数
	return x == 0 ? 1 : (32 - __clz(x));
}

//返回一个整数的二进制长度，0时返回0
__device__ 
int countNum_abc(unsigned int x) {
	return x == 0 ? 0 : (32 - __clz(x));
}

/*
以下两个函数为编码方法，原先方案均存储字符串
现在由于考虑效率和CUDA的限制，在kernel中先将结果保存为int型
传回Host后通过位操作转为字符串存储
*/
//修正的二元编码,标志位+codeword 
__device__ 
Code_Len abc(int x, int L, int H) {

	int delta = H - L;
	int range = delta + 1;
	float log2range = log2f(range);
	float ub = ceilf(log2range);
	float lb = floorf(log2range);
	float th = powf(2, ub) - range;
	int residual = x - L;
	Code_Len binCode_S;

	if (residual >= th) {
		residual += th;
		binCode_S.lenth = ub + 1;
	}
	else
		binCode_S.lenth = lb + 1;

	binCode_S.code = residual;
	return binCode_S;
}

//Golomb-Rice编码，最后的编码为grc标志位+unary一元编码+0+binary二元编码
__device__ 
Code_Len grc(int x, int L, int H) {
	int k = 2;  //固定k参数
	int length_Unary;    //记录二进制化后的商长度
	unsigned int binary;//二元编码部分，即目标值的余数
	unsigned int u;//商,unary中1的个数
	unsigned int unary;
	Code_Len codewords;

	unsigned int residual;
	if (x < L) {
		residual = L - x - 1; //处于下界的目标值
		codewords.code = 2;
	}
	else {
		residual = x - H - 1; //处于上界的目标值
		codewords.code = 3;
	}
	//codewords.lenth = 2;

	int len = countNum(residual);
	//商为residual/2^k 向下取整，通过位操作>>k位，被移除的二进制部分即为余数或模
	//商就是unary中1的位数
	if (len <= k) //若余项二进制长度小于k,移位后为0，长度为0
		length_Unary = 0;
	else          //大于k
		length_Unary = len - k;//商的长度，一元编码长度

	binary = residual & ((1 << k) - 1);//取得上述移位时被移除的部分，即余数
	u = residual >> k;//移除余数部分，取得商，若len<=k,则u=0

	unary = (1 << u) - 1;//unary是商的数值个1，所以其编码长度即d，1移位d，减1
	unary = unary << 1 | 1;//加0作为一元和二元编码的分界标志

	//codewords.lenth = u + 3 + (len - length_Unary);
	codewords.lenth = u + 3 + k;

	if (codewords.lenth > 16) {
		codewords.code = (codewords.code << 14) | x;	//后8bit就是像素值的二进制
		codewords.lenth = 16;
		return codewords;
	}

	codewords.code = (codewords.code << ((u + 1) + k)) | (unary << k) | binary;

	//codewords.code = (codewords.code << ((u + 1) + (len - length_Unary))) | (unary << (len - length_Unary)) | binary;
	//codewords.code = ( unary << (2 + len - length_Unary) ) | (binary << 2) | (flag << 1);
	
	return codewords;
}

//内核，编码过程全在GPU—CUDA上运行
__global__ 
void Kernel(int rows, int cols, int imageSize, uchar* data, /*unsigned short* code_out, 
	unsigned short* lenth_out, unsigned short* lendata,*/ unsigned char* g_obitres) 
{
	//init shared memory
	__shared__ unsigned char byteStream[8*blockSize];			//byte stream
	__shared__ unsigned char bitStream[blockSize];			//bit stream
	__shared__ unsigned short prefix[blockSize];				//prefix
	__shared__ unsigned char binarylen[blockSize];			//code bit length
	__shared__ unsigned short blockLen;					//total length per block

	int tid = threadIdx.x; //当前块内的线程号
	int index = (blockIdx.x * blockDim.x) + threadIdx.x; //总线程计数编号
	/***************************************************************************/
	if (index < imageSize) {
		/******************************* Encoding *******************************/
		int N1, N2;
		Code_Len codeword;
		//预测模型
		if (index < 2) {//case1，相当于原先算法中的前两个像素的编码
					//直接记录原值，返回Host后转为二进制
			codeword.code = data[index];
			codeword.lenth = 8;
		}
		else if (index < cols) {//case2，首行的其余像素的编码
			N1 = data[index - 2];
			N2 = data[index - 1];
		}
		else if (index % cols == 0) {//case3，除首行外，所有的首列像素点
			N1 = data[index - cols];
			N2 = data[index - cols + 1];
		}
		else {//case4，其余
			N1 = data[index - 1];
			N2 = data[index - cols];
		}
		//编码方式选取并编码
		if (index > 1) {//非前两个像素的线程
			int L = min(N1, N2);
			int H = max(N1, N2);
			int P = data[index];
			if (P<L || P>H)
				codeword = grc(P, L, H);
			else
				codeword = abc(P, L, H);
		}
		binarylen[tid] = codeword.lenth;
		__syncthreads();
		/******************************** scan to get prefix **********************************/
		int offset = 1;
		bitStream[tid] = 0;
		prefix[tid] = codeword.lenth;		// load binary to l1 cache
		int n = blockSize, tid_offset = tid << 1; 
		for (int d = n >> 1; d > 0; d >>= 1) {
			__syncthreads();
			if (tid < d) {
				int ai = offset * (tid_offset + 1) - 1;
				int bi = offset * (tid_offset + 2) - 1;
				prefix[bi] += prefix[ai];
			}
			offset <<= 1;
		}
		if (tid == 0) prefix[n - 1] = 0;
		for (int d = 1; d < n; d <<= 1) {
			offset >>= 1;
			__syncthreads();
			if (tid < d) {
				int ai = offset * (tid_offset + 1) - 1;
				int bi = offset * (tid_offset + 2) - 1;
				unsigned short t = prefix[ai];
				prefix[ai] = prefix[bi];
				prefix[bi] += t;
			}
		}
		__syncthreads();
		if (tid == 0) {
			blockLen = prefix[n - 1] + binarylen[n - 1];
			//注意与实际bit长度不同，最后一个字节默认补足8bit
			unsigned short block_bytenum = blockLen;
			//unsigned short block_bytenum = (blockLen & 7) == 0 ? (blockLen >> 3) : ((blockLen >> 3) + 1);
			bitStream[blockSize-1] = block_bytenum & 0xFF;
			bitStream[blockSize-2] = (block_bytenum >> 8) & 0xFF;
		}
		__syncthreads();
		/******************************** byte stream **********************************/
		unsigned short d = codeword.code;
		for (int i = prefix[tid] + codeword.lenth - 1; i >= prefix[tid]; --i) {
			byteStream[i] = d & 1;
			d >>= 1;
		}
		__syncthreads();
		/******************************** bit stream **********************************/
		if ((tid << 3) < blockLen) {
			int byteIndex = (tid << 3) + 7;
			for (int i = 7; i >= 0 && (byteIndex - i) < blockLen; --i)
				bitStream[tid] |= (byteStream[byteIndex - i] << i);
		}
		g_obitres[index] = bitStream[tid];
	}
}

//封装内核函数接口，给cpp调用
void FELICS(int& rows, int& cols, uchar* data, unsigned char* g_obitres, cudaStream_t* stream) {
	dim3 block(blockSize, 1);
	dim3 grid(gridSize, 1);
	Kernel << < grid, block, 0 , *stream>> > (I.rows, I.cols, pixelSize, data, g_obitres);
}


