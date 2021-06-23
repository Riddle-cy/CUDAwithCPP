
#include <ctime>

#include "device_launch_parameters.h"
#include "device_functions.h"
#include "cuda.h"
#include "common.h"
#include "cuda_runtime.h"
#include <cuda_profiler_api.h>

//����ʱ�����ȴ洢�����int�ͣ���ȡ��Ӧ�����Ƶĳ���,__clz()ΪCUDA�ڽ�����
//����һ�������Ķ����Ƴ��ȣ�0ʱ����1
__device__ 
int countNum(unsigned int x) {
	//����x��Ӧ�Ķ�����ֵ��λ��,Ϊ0ʱ����1��__clz()Ϊcuda�⺯��
	return x == 0 ? 1 : (32 - __clz(x));
}

//����һ�������Ķ����Ƴ��ȣ�0ʱ����0
__device__ 
int countNum_abc(unsigned int x) {
	return x == 0 ? 0 : (32 - __clz(x));
}

/*
������������Ϊ���뷽����ԭ�ȷ������洢�ַ���
�������ڿ���Ч�ʺ�CUDA�����ƣ���kernel���Ƚ��������Ϊint��
����Host��ͨ��λ����תΪ�ַ����洢
*/
//�����Ķ�Ԫ����,��־λ+codeword 
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

//Golomb-Rice���룬���ı���Ϊgrc��־λ+unaryһԪ����+0+binary��Ԫ����
__device__ 
Code_Len grc(int x, int L, int H) {
	int k = 2;  //�̶�k����
	int length_Unary;    //��¼�����ƻ�����̳���
	unsigned int binary;//��Ԫ���벿�֣���Ŀ��ֵ������
	unsigned int u;//��,unary��1�ĸ���
	unsigned int unary;
	Code_Len codewords;

	unsigned int residual;
	if (x < L) {
		residual = L - x - 1; //�����½��Ŀ��ֵ
		codewords.code = 2;
	}
	else {
		residual = x - H - 1; //�����Ͻ��Ŀ��ֵ
		codewords.code = 3;
	}
	//codewords.lenth = 2;

	int len = countNum(residual);
	//��Ϊresidual/2^k ����ȡ����ͨ��λ����>>kλ�����Ƴ��Ķ����Ʋ��ּ�Ϊ������ģ
	//�̾���unary��1��λ��
	if (len <= k) //����������Ƴ���С��k,��λ��Ϊ0������Ϊ0
		length_Unary = 0;
	else          //����k
		length_Unary = len - k;//�̵ĳ��ȣ�һԪ���볤��

	binary = residual & ((1 << k) - 1);//ȡ��������λʱ���Ƴ��Ĳ��֣�������
	u = residual >> k;//�Ƴ��������֣�ȡ���̣���len<=k,��u=0

	unary = (1 << u) - 1;//unary���̵���ֵ��1����������볤�ȼ�d��1��λd����1
	unary = unary << 1 | 1;//��0��ΪһԪ�Ͷ�Ԫ����ķֽ��־

	//codewords.lenth = u + 3 + (len - length_Unary);
	codewords.lenth = u + 3 + k;

	if (codewords.lenth > 16) {
		codewords.code = (codewords.code << 14) | x;	//��8bit��������ֵ�Ķ�����
		codewords.lenth = 16;
		return codewords;
	}

	codewords.code = (codewords.code << ((u + 1) + k)) | (unary << k) | binary;

	//codewords.code = (codewords.code << ((u + 1) + (len - length_Unary))) | (unary << (len - length_Unary)) | binary;
	//codewords.code = ( unary << (2 + len - length_Unary) ) | (binary << 2) | (flag << 1);
	
	return codewords;
}

//�ںˣ��������ȫ��GPU��CUDA������
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

	int tid = threadIdx.x; //��ǰ���ڵ��̺߳�
	int index = (blockIdx.x * blockDim.x) + threadIdx.x; //���̼߳������
	/***************************************************************************/
	if (index < imageSize) {
		/******************************* Encoding *******************************/
		int N1, N2;
		Code_Len codeword;
		//Ԥ��ģ��
		if (index < 2) {//case1���൱��ԭ���㷨�е�ǰ�������صı���
					//ֱ�Ӽ�¼ԭֵ������Host��תΪ������
			codeword.code = data[index];
			codeword.lenth = 8;
		}
		else if (index < cols) {//case2�����е��������صı���
			N1 = data[index - 2];
			N2 = data[index - 1];
		}
		else if (index % cols == 0) {//case3���������⣬���е��������ص�
			N1 = data[index - cols];
			N2 = data[index - cols + 1];
		}
		else {//case4������
			N1 = data[index - 1];
			N2 = data[index - cols];
		}
		//���뷽ʽѡȡ������
		if (index > 1) {//��ǰ�������ص��߳�
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
			//ע����ʵ��bit���Ȳ�ͬ�����һ���ֽ�Ĭ�ϲ���8bit
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

//��װ�ں˺����ӿڣ���cpp����
void FELICS(int& rows, int& cols, uchar* data, unsigned char* g_obitres, cudaStream_t* stream) {
	dim3 block(blockSize, 1);
	dim3 grid(gridSize, 1);
	Kernel << < grid, block, 0 , *stream>> > (I.rows, I.cols, pixelSize, data, g_obitres);
}


