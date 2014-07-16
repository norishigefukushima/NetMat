#pragma once

#include "NetUDP.h"
#include <opencv2/opencv.hpp>

using namespace std;
using namespace cv;


#define PACKET_INFO 0

#define PACKET_USER_DEFINE_PARAMETER 8
#define NET_OPENCV_HEADERSIZE (18+4*PACKET_USER_DEFINE_PARAMETER)

struct PacketHeader
{
	int64 sequence_number;
	int64 timestamp;
	uchar fragment_number;
	uchar fragment_number_max;
	float parameter[PACKET_USER_DEFINE_PARAMETER];
	PacketHeader();
	PacketHeader(int64 sequence_number_, int64 timestamp_, uchar fragment_number_, uchar fragment_number_max_);
	PacketHeader(int64 sequence_number_, int64 timestamp_, uchar fragment_number_, uchar fragment_number_max_, float* param);
	void show();
};

class NetMat : public NetUDP
{
private:
	char* packet;
	vector<uchar> prev;

	void getPacketHeader(const char* src, PacketHeader& phead);
	int makePacket(char* src , const int data_size, PacketHeader& phead);
	int fragmented_send(vector<uchar>& src, const int64 timestamp);
	bool findIndex(int64& seqindex, int& fragment_max);

	int64 getHeadIndex();

	enum
	{
		ELSE_GRAB_PREV_OUTPUT=0,
		ELSE_GRAB_HEAD,
	};
	//return size of data
	int grabIndex(vector<uchar>& dest, const int64 index , int64& sequenceIndex, int mode, int waitLoopMax =15);
	//return recv size
	int grabHead(vector<uchar>& dest, int64& sequenceIndex);
	int fragmented_recv(vector<uchar>& dest);

public:
	int64 sequence_number;
	int64 prev_sequence_number;
	void initRingBuffer();

	NetMat(char* src_ip, int src_port, char* sin_ip, int sin_port);
	~NetMat();

	void waitReady();

	int sendMat(const Mat& src, int option=CV_IMWRITE_JPEG_QUALITY, int parameter = 100);
	int64 recvMatIndex(Mat& dest, int64 index, int flags=IMREAD_UNCHANGED);
	int64 recvMatNext(Mat& dest, int flags=IMREAD_UNCHANGED);
	int64 recvMatHead(Mat& dest,int flags=IMREAD_UNCHANGED);
};