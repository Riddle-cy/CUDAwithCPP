#include <iostream>
#include <fstream>
#include <ctime>
#include <vector>
#include <windows.h>
#include <thread>
#include <string.h>
#include <vector>

using namespace std;

#include <EGenTL.h>
#include <EGrabber.h>
#include "cuda.h"
#include "grabber_cuda.h"
#include "myBit.h"
#include "common.h"

unsigned char bitTable[65536] = {};

#define StreamNUM 3
#define FIFONUM 2

int overtime = 0;

cv::Mat I;
unsigned int imageSize;									//图像大小
vector<uchar*> d_in(StreamNUM);
vector<unsigned char*> cd_in(StreamNUM);
vector<unsigned char*> g_obitres(StreamNUM);
vector<FIFO*> bitFIFO(FIFONUM);
L1_FIFO* dataFIFO;
vector<myFileBuffer*> fileFIFO(FIFONUM);
vector<string> Path(FIFONUM);
cudaStream_t myStream[StreamNUM];

unsigned short fileType = 0x4D42;
BmpFileHeader bmpFileHeader;
BmpInfoHeader bmpInfoHeader;
RgbQuad* quad;
int offset;
int bmp_width;
int bmp_height;
int picSize;

Euresys::Buffer* currentBuffer = nullptr;
MyGrabber* grabber = nullptr;

cudaEvent_t Tstart, Tstop;//通过cudaEvent_t精确获取GPU中的运行时间，即编码时间//事件记录
float elapsedTime = 0.0;
float average = 0.0;

bool comFinish = false;

FIFO resFIFO(10);

void initCudaBuffer() {		
	string disk0 = "H:\\compress_";
	string disk1 = "J:\\compress_";
	dataFIFO = new L1_FIFO(L1_NUM);
	for (int i = 0; i < FIFONUM; ++i) {
		bitFIFO[i] = new FIFO(L2_NUM);
		if (i & 1)
			Path[i] = disk1;
		else
			Path[i] = disk0;
	}
	//分配cuda系列的内存，传输进行页锁定分配
	for (int i = 0; i < StreamNUM; ++i) {
		cudaMallocHost((void**)&d_in[i], pixelSize * sizeof(uchar));
		cudaMallocHost((void**)&cd_in[i], pixelSize * sizeof(unsigned char));
		cudaMalloc((void**)&g_obitres[i], pixelSize * sizeof(unsigned char));
		cudaStreamCreate(&myStream[i]);
	}
	cudaEventCreate(&Tstart);
	cudaEventCreate(&Tstop);
}

void destroyRes() {

	for (int i = 0; i < FIFONUM; ++i)
		delete bitFIFO[i];

	for (int i = 0; i < StreamNUM; ++i) {
		cudaFreeHost(d_in[i]);
		cudaFree(g_obitres[i]);
		cudaStreamDestroy(myStream[i]);
	}
}

float comTime[CaptureNUM / 10 + 1] = {0.0};

void compressThread(uchar* data) {
	bool exit = false;
	int count = 0;
	myWatch comWatch;
	comWatch.Start();
	while(!exit) {
		if (resFIFO.isFull()) {
			//printf("DataFIFO is full!\n");
			continue;
		}
		if (data) {
			if (count == 0) {
				//cudaEventRecord(Tstart, 0);
				cudaMemcpyAsync(d_in[0], data, imageSize * sizeof(uchar), cudaMemcpyHostToDevice, myStream[0]);
				//FELICS(I.rows, I.cols, d_in[0], g_obitres[0], &myStream[0]);
				//cudaEventRecord(Tstop, 0);
				//cudaEventSynchronize(Tstop);
				//cudaEventElapsedTime(&elapsedTime, Tstart, Tstop);
				//printf("kernel time：%fms\n", elapsedTime);
				//average += elapsedTime;
			}
			else if (count == 1) {
				//cudaEventRecord(Tstart, 0);
				cudaMemcpyAsync(d_in[1], data, imageSize * sizeof(uchar), cudaMemcpyHostToDevice, myStream[1]);
				FELICS(I.rows, I.cols, d_in[0], g_obitres[0], &myStream[0]);
				/*cudaEventRecord(Tstop, 0);
				cudaEventSynchronize(Tstop);
				cudaEventElapsedTime(&elapsedTime, Tstart, Tstop);
				printf("kernel time：%fms\n", elapsedTime);
				average += elapsedTime;*/
			}
			else {
				resFIFO.bufferPtr[resFIFO.rear].index = count - 2;
				//Host To Device
				cudaEventRecord(Tstart, 0);
				cudaMemcpyAsync(d_in[count % StreamNUM], data, imageSize * sizeof(uchar), 
					cudaMemcpyHostToDevice, myStream[count % StreamNUM]);
				//调用内核进行编码
				FELICS(I.rows, I.cols, d_in[(count-1) % StreamNUM], g_obitres[(count-1) % StreamNUM],
					&myStream[(count-1) % StreamNUM]);
				//将结果从Device拷贝至Host
				cudaMemcpyAsync(resFIFO.bufferPtr[resFIFO.rear].ptr, g_obitres[(count - 2) % StreamNUM], 
					imageSize * sizeof(unsigned char), cudaMemcpyDeviceToHost, myStream[(count - 2) % StreamNUM]);
				cudaEventRecord(Tstop, 0);
				cudaEventSynchronize(Tstop);
				cudaEventElapsedTime(&elapsedTime, Tstart, Tstop);
				printf("kernel time：%fms\n", elapsedTime);
				average += elapsedTime;
				resFIFO.rear = (resFIFO.rear + 1) % resFIFO.size();
			}
		}
		else
			printf("data error\n");

		if (++count == CaptureNUM + 1) exit = true;
		if (count % 10 == 0) {
			comWatch.End();
			comTime[count/10 - 1] = comWatch.getTime();
			comWatch.Start();
		}
	}
	printf("Average time %lf\n", average / count);

	comFinish = true;
	resFIFO.changeFlag();
}

void storeThread() {
	myWatch watchS;
	while (!resFIFO.isExit() || !resFIFO.isEmpty()) {
		
		if (resFIFO.isEmpty()) {
			printf("DataFIFO is empty!\n");
			continue;
		}
		int index = resFIFO.bufferPtr[resFIFO.front].index;
		FILE* pfile = fopen((to_string(index) + ".dat").c_str(), "wb");
		if (!pfile) {
			++resFIFO.fileFail;
			continue;
		}
		
		unsigned char* h_bitres = resFIFO.bufferPtr[resFIFO.front].ptr;
		for (int i = 0; i < gridSize; ++i) {
			unsigned int b = i << 9;
			unsigned short len_temp = h_bitres[b + blockSize-2];
			len_temp = (len_temp << 8) | (h_bitres[b + blockSize-1]);	//bit length
			unsigned char rear_bit = len_temp & 7;	//最后一个字节内有效比特数
			unsigned short byteNUM = (len_temp >> 3) + 2;
			h_bitres[b + byteNUM - 2] = rear_bit;
			fwrite((h_bitres + b), 1, byteNUM, pfile);
		}
		watchS.Start();
		
		fclose(pfile);

		watchS.End();
		printf("Storing time : %lf\n", watchS.getTime());

		resFIFO.front = (resFIFO.front + 1) % resFIFO.size();

		int codelen = 0;
		for (int i = 0; i < gridSize; ++i) {
			int b = i << 9;
			unsigned short len_temp = h_bitres[b + blockSize-2];
			len_temp = (len_temp << 8) | (h_bitres[b + blockSize-1]);
			codelen += len_temp;
		}
		double CR = (double)(imageSize) * 8 / (double)(codelen);
		cout << "CR : " << setprecision(8) << CR << endl;
	}
	return;
}

void MyGrabber::onNewBufferEvent(const Euresys::NewBufferData& data) {
	watch.Start();
	Euresys::Buffer b(data);
	unsigned char* buf = b.getInfo<uint8_t*>(*this, Euresys::gc::BUFFER_INFO_BASE);

	if (dataFIFO->isFull()) {
		printf("L1 FIFO is full!\n");
		++lostframe;
	}
	else {
		dataFIFO->bufferPtr[dataFIFO->rear].index = count;
		memcpy(dataFIFO->bufferPtr[dataFIFO->rear].ptr, buf, pixelSize);
		dataFIFO->rear = (dataFIFO->rear + 1) % L1_NUM;
		printf("Compressing image %d\n", count);
		++count;
	}

	//if (count == 0) {
	//	cudaMemcpyAsync(d_in[0], buf, pixelSize * sizeof(uchar), cudaMemcpyHostToDevice, myStream[0]);
	//	++count;
	//}
	//else if (count == 1) {
	//	cudaMemcpyAsync(d_in[1], buf, pixelSize * sizeof(uchar), cudaMemcpyHostToDevice, myStream[1]);
	//	FELICS(I.rows, I.cols, d_in[0], g_obitres[0], &myStream[0]);
	//	++count;
	//}
	//else {
	//	FIFO* FIFO_ptr = bitFIFO[(count - 2) & 1];
	//	if (FIFO_ptr->isFull()) {  
	//		printf("DataFIFO is full!\n");
	//	}
	//	else {
	//		FIFO_ptr->bufferPtr[FIFO_ptr->rear].index = count - 2;
	//		cudaMemcpyAsync(d_in[count % StreamNUM], buf, pixelSize * sizeof(uchar),
	//			cudaMemcpyHostToDevice, myStream[count % StreamNUM]);
	//		//调用内核进行编码
	//		FELICS(I.rows, I.cols, d_in[(count - 1) % StreamNUM], g_obitres[(count - 1) % StreamNUM],
	//			&myStream[(count - 1) % StreamNUM]);
	//		//将结果从Device拷贝至Host
	//		cudaMemcpyAsync(FIFO_ptr->bufferPtr[FIFO_ptr->rear].ptr, g_obitres[(count - 2) % StreamNUM],
	//			pixelSize * sizeof(unsigned char), cudaMemcpyDeviceToHost, myStream[(count - 2) % StreamNUM]);
	//		FIFO_ptr->rear = (FIFO_ptr->rear + 1) % FIFO_ptr->size();
	//		printf("Compressing image %d\n", count++);
	//	}
	//}

	b.push(*this);
	watch.End();
	if (watch.getTime() > 0.002000)
		++overtime;
	//cout << watch.getTime() << endl;
}

void solveData() {
	int num = 0;
	while (!dataFIFO->isExit() || !dataFIFO->isEmpty()) {
		if (dataFIFO->isEmpty())
			continue;
		else {
			int index = dataFIFO->bufferPtr[dataFIFO->front].index;
			FIFO* bp = bitFIFO[index & 1];
			unsigned char* buf = dataFIFO->bufferPtr[dataFIFO->front].ptr;
			if (index == 0) {
				cudaMemcpyAsync(cd_in[0], buf, pixelSize * sizeof(uchar), cudaMemcpyHostToDevice, myStream[0]);
			}
			else if (index == 1) {
				cudaMemcpyAsync(cd_in[1], buf, pixelSize * sizeof(uchar), cudaMemcpyHostToDevice, myStream[1]);
				FELICS(I.rows, I.cols, cd_in[0], g_obitres[0], &myStream[0]);
			}
			else {
				if (bp->isFull()) {
					printf("bitFIFO %d is full!\n", index & 1);
					bp->addFull();
					continue;
				}
				//cudaEventRecord(Tstart, 0);
				cudaMemcpyAsync(cd_in[index % StreamNUM], buf, pixelSize * sizeof(uchar),
				cudaMemcpyHostToDevice, myStream[index % StreamNUM]);
				//调用内核进行编码
				FELICS(I.rows, I.cols, cd_in[(index - 1) % StreamNUM], g_obitres[(index - 1) % StreamNUM],
					&myStream[(index - 1) % StreamNUM]);
				//将结果从Device拷贝至Host
				cudaMemcpyAsync(bp->bufferPtr[bp->rear].ptr, g_obitres[(index - 2) % StreamNUM],
					pixelSize * sizeof(unsigned char), cudaMemcpyDeviceToHost, myStream[(index - 2) % StreamNUM]);
				/*cudaEventRecord(Tstop, 0);
				cudaEventSynchronize(Tstop);*/
				bp->rear = (bp->rear + 1) % bp->size();
			}
			dataFIFO->front = (dataFIFO->front + 1) % dataFIFO->size();
			++num;
		}
	}
	printf("Solve thread exit, total %d images.\n", num);
}

float writeTime[2][CaptureNUM / 20 + 1];

void WriteThread(FIFO* dataBuffer, int id) {
	int num = 0;
	FILE* pfile = fopen((Path[id] + to_string(id) + ".dat").c_str(), "wb");
	myWatch writeW;
	int count = 0;
	writeW.Start();
	while (!dataBuffer->isExit() || !dataBuffer->isEmpty()) {
		
		if (dataBuffer->isEmpty()) {
			//printf("Bit FIFO %d is empty.\n", id);
			continue;
		}
		else {
			unsigned char* h_bitres = dataBuffer->bufferPtr[dataBuffer->front].ptr;
			for (int i = 0; i < gridSize; ++i) {
				unsigned int b = i << 10;
				//每个block最后两个字节保存了当前block的总bit数量
				unsigned short len_temp = h_bitres[b + blockSize - 2];
				len_temp = (len_temp << 8) | (h_bitres[b + blockSize - 1]);	//bit length
				unsigned char rear_bit = len_temp & 7;	//最后一个字节内有效比特数
				unsigned short byteNUM = (len_temp >> 3) + 2;//多两个字节作为block分割
				h_bitres[b + byteNUM - 2] = rear_bit;//倒数第二个存最后一个字节，最后一个全0 ，分割线
				fwrite((h_bitres + b), 1, byteNUM, pfile);
			}
			 
			/*int codelen = 0;
			for (int i = 0; i < gridSize; ++i) {
				int b = i << 10;
				unsigned short len_temp = h_bitres[b + blockSize-2];
				len_temp = (len_temp << 8) | (h_bitres[b + blockSize-1]);
				codelen += len_temp;
			}
			double CR = (double)(pixelSize) * 8 / (double)(codelen);
			cout << "CR : " << setprecision(8) << CR << endl;*/

			dataBuffer->front = (dataBuffer->front + 1) % dataBuffer->size();
			++num;
			/*writeW.End();
			cout << writeW.getTime() << endl;*/

			if (++count % 10 == 0) {
				writeW.End();
				writeTime[id][count / 10 - 1] = writeW.getTime();
				writeW.Start();
			}
		}
	}
	fclose(pfile);

	ofstream opt;
	opt.open("I:\\WriteTime" + to_string(id) + ".csv", ios::out | ios::trunc);
	for (int i = 0; i < CaptureNUM / 20; ++i) {
		opt << writeTime[id][i] << endl;
	}
	opt.close();

	printf("Exit WriteThread_%d, total %d images.\n", id, num);
}

bool Simulation() {
	//初始化CUDA
	if (!InitCUDA())
		return false;
	printf("CUDA initialized.\n");

	I = cv::imread("test_0.bmp", 0);
	imageSize = I.rows * I.cols;//图像的像素点总数
	printf("Image Size: %d*%d\n", I.rows, I.cols);
	uchar* data = I.ptr<uchar>(0);
	myWatch watch;
	/****************************单张图像进行并行压缩测试，包括数据传输时间********************************/
	printf("\nTrue Test:\n");
	//分配记录的存储空间, 存储从GPU传回的数据
	initCudaBuffer();
	thread compress(compressThread, data);
	thread write(storeThread);
	compress.join();
	write.join();
	//计算压缩率,可改为GPU进行规约加法计算
	//释放
	destroyRes();
	return 1;
}

bool RealTest() {
	if (!InitCUDA())
		return false;
	printf("CUDA initialized.\n");

	try {
		Euresys::EGenTL gentl;
		MyGrabber grabber_C(gentl);
		grabber = &grabber_C;
		myWatch watchCamera;
		//initBmpHead(grabber);
		initCudaBuffer();

		vector<thread> writeT(FIFONUM);
		for (int i = 0; i < FIFONUM; ++i) {
			writeT[i] = thread(WriteThread, bitFIFO[i], i);
		}
		thread getData(solveData);
		getData.detach();
		for (int i = 0; i < FIFONUM; ++i) {
			writeT[i].detach();
		}

		grabber->start();
		
		/*while (1) {
			watchCamera.End();
			if (watchCamera.getTime() - 60 > 0.000001) {
				grabber->~MyGrabber();
				for (int i = 0; i < FIFONUM; ++i) {
					bitFIFO[i]->changeFlag();
					fileFIFO[i]->changeFlag();
				}
				break;
			}
		}*/

		while (1) {
			if(grabber->picNUM() <= 1000)
				watchCamera.Start();
			if (grabber->picNUM() >= CaptureNUM) {
				for (int i = 0; i < FIFONUM; ++i) {
					bitFIFO[i]->changeFlag();
				}
				grabber->stop();
				cout << "overtime " << overtime << endl;
				cout << "lost frame " << grabber->lostFrame() << endl;
				for (int i = 0; i < FIFONUM; ++i) {
					cout << "bitFIFO " << i << " full time " << bitFIFO[i]->getFullNUM() << endl;
				}
				break;
			}
		}

		watchCamera.End();
		cout << watchCamera.getTime() << endl;
		destroyRes();
	}
	catch (const std::exception& e) {
		std::cout << "grab error" << e.what() << std::endl;
	}

	return true;
}

void Modify_RealTest() {
	if (!InitCUDA()) {
		printf("CUDA init failed!\n");
		return;
	}
	printf("CUDA initialized.\n");

	try {
		Euresys::EGenTL gentl;
		MyGrabber grabber_C(gentl);
		grabber = &grabber_C;
		initCudaBuffer();
		myWatch watchCap;
		vector<thread> writeT(FIFONUM);
		for (int i = 0; i < FIFONUM; ++i) {
			writeT[i] = thread(WriteThread, bitFIFO[i], i);
			writeT[i].detach();
		}
		myWatch comWatch;
		int count = 0;
		grabber->start();
		watchCap.Start();
		comWatch.Start();
		while (!dataFIFO->isExit() || !dataFIFO->isEmpty()) {
			if (grabber->picNUM() >= CaptureNUM) {
				if (!dataFIFO->isExit()) {
					watchCap.End();
					dataFIFO->changeFlag();
					grabber->stop();
					for (int i = 0; i < FIFONUM; ++i) {
						bitFIFO[i]->changeFlag();
					}
				}
			}
			if (dataFIFO->isEmpty()) {
				//printf("FIFO is empty!\n");
				continue;
			}
			else {
				int index = dataFIFO->bufferPtr[dataFIFO->front].index;
				FIFO* bp = bitFIFO[index & 1];
				unsigned char* buf = dataFIFO->bufferPtr[dataFIFO->front].ptr;
				if (index == 0) {
					cudaMemcpyAsync(cd_in[0], buf, pixelSize * sizeof(uchar), cudaMemcpyHostToDevice, myStream[0]);
				}
				else if (index == 1) {
					cudaMemcpyAsync(cd_in[1], buf, pixelSize * sizeof(uchar), cudaMemcpyHostToDevice, myStream[1]);
					FELICS(I.rows, I.cols, cd_in[0], g_obitres[0], &myStream[0]);
				}
				else {
					if (bp->isFull()) {
						printf("bitFIFO %d is full!\n", index & 1);
						bp->addFull();
						continue;
					}
					cudaEventRecord(Tstart, 0);
					cudaMemcpyAsync(cd_in[index % StreamNUM], buf, pixelSize * sizeof(uchar),
						cudaMemcpyHostToDevice, myStream[index % StreamNUM]);
					//调用内核进行编码
					FELICS(I.rows, I.cols, cd_in[(index - 1) % StreamNUM], g_obitres[(index - 1) % StreamNUM],
						&myStream[(index - 1) % StreamNUM]);
					//将结果从Device拷贝至Host
					cudaMemcpyAsync(bp->bufferPtr[bp->rear].ptr, g_obitres[(index - 2) % StreamNUM],
						pixelSize * sizeof(unsigned char), cudaMemcpyDeviceToHost, myStream[(index - 2) % StreamNUM]);
					cudaEventRecord(Tstop, 0);
					cudaEventSynchronize(Tstop);
					bp->rear = (bp->rear + 1) % bp->size();
				}
				dataFIFO->front = (dataFIFO->front + 1) % dataFIFO->size();
			}

			if (++count % 10 == 0) {
				comWatch.End();
				comTime[count / 10 - 1] = comWatch.getTime();
				comWatch.Start();
			}
		}	

		cout << "overtime " << overtime << endl;
		cout << "lost frame " << grabber->lostFrame() << endl;
		for (int i = 0; i < FIFONUM; ++i) {
			cout << "bitFIFO " << i << " full time " << bitFIFO[i]->getFullNUM() << endl;
		}

		cout << watchCap.getTime() << endl;

		ofstream opt;
		opt.open("I:\\CompressTime.csv", ios::out | ios::trunc);
		for (int i = 0; i < CaptureNUM / 10; ++i) {
			opt << comTime[i] << endl;
		}
		opt.close();

		destroyRes();
	}
	catch (const std::exception& e) {
		std::cout << "grab error" << e.what() << std::endl;
	}
}

void testFixTime() {
	if (!InitCUDA()) {
		printf("CUDA init failed!\n");
		return;
	}
	printf("CUDA initialized.\n");

	try {
		Euresys::EGenTL gentl;
		MyGrabber grabber_C(gentl);
		grabber = &grabber_C;
		initCudaBuffer();
		myWatch watchCap;
		vector<thread> writeT(FIFONUM);
		for (int i = 0; i < FIFONUM; ++i) {
			writeT[i] = thread(WriteThread, bitFIFO[i], i);
			writeT[i].detach();
		}

		double realTime;
		grabber->start();
		watchCap.Start();

		while (!dataFIFO->isExit() || !dataFIFO->isEmpty()) {
			watchCap.End();
			if (watchCap.getTime() >= CaptureTIME) {
				if (!dataFIFO->isExit()) {
					realTime = watchCap.getTime();
					dataFIFO->changeFlag();
					grabber->stop();
					for (int i = 0; i < FIFONUM; ++i) {
						bitFIFO[i]->changeFlag();
					}
				}
			}
			if (dataFIFO->isEmpty()) {
				//printf("FIFO is empty!\n");
				continue;
			}
			else {
				int index = dataFIFO->bufferPtr[dataFIFO->front].index;
				FIFO* bp = bitFIFO[index & 1];
				unsigned char* buf = dataFIFO->bufferPtr[dataFIFO->front].ptr;
				if (index == 0) {
					cudaMemcpyAsync(cd_in[0], buf, pixelSize * sizeof(uchar), cudaMemcpyHostToDevice, myStream[0]);
				}
				else if (index == 1) {
					cudaMemcpyAsync(cd_in[1], buf, pixelSize * sizeof(uchar), cudaMemcpyHostToDevice, myStream[1]);
					FELICS(I.rows, I.cols, cd_in[0], g_obitres[0], &myStream[0]);
				}
				else {
					if (bp->isFull()) {
						printf("bitFIFO %d is full!\n", index & 1);
						bp->addFull();
						continue;
					}
					cudaEventRecord(Tstart, 0);
					cudaMemcpyAsync(cd_in[index % StreamNUM], buf, pixelSize * sizeof(uchar),
						cudaMemcpyHostToDevice, myStream[index % StreamNUM]);
					//调用内核进行编码
					FELICS(I.rows, I.cols, cd_in[(index - 1) % StreamNUM], g_obitres[(index - 1) % StreamNUM],
						&myStream[(index - 1) % StreamNUM]);
					//将结果从Device拷贝至Host
					cudaMemcpyAsync(bp->bufferPtr[bp->rear].ptr, g_obitres[(index - 2) % StreamNUM],
						pixelSize * sizeof(unsigned char), cudaMemcpyDeviceToHost, myStream[(index - 2) % StreamNUM]);
					cudaEventRecord(Tstop, 0);
					cudaEventSynchronize(Tstop);
					bp->rear = (bp->rear + 1) % bp->size();
				}
				dataFIFO->front = (dataFIFO->front + 1) % dataFIFO->size();
			}
		}
		cout << "Runtime " << realTime << endl;
		cout << "overtime " << overtime << endl;
		cout << "lost frame " << grabber->lostFrame() << endl;
		for (int i = 0; i < FIFONUM; ++i) {
			cout << "bitFIFO " << i << " full time " << bitFIFO[i]->getFullNUM() << endl;
		}

		cout << watchCap.getTime() << endl;
		destroyRes();
	}
	catch (const std::exception& e) {
		std::cout << "grab error" << e.what() << std::endl;
	}
}

int main(int argc, char **argv) {
	int exitCode = 0;
	
	Simulation();

	//RealTest();

	//Modify_RealTest();

	//testFixTime();

	system("pause");
	return exitCode;
}


