#pragma once

#include <WinSock2.h>
#include <windows.h>

#include <vector>

#pragma comment(lib, "ws2_32.lib")

#define NET_UDP_SEND_MAX 65507
//#define NET_UDP_SEND_MAX 6550
#define RING_BUFFER_SIZE 60

class RingBuffer
{
private:
	CRITICAL_SECTION section;
	char* dataPtr;
	int head;
	int tail;
	bool isInit;

public:
	int size;
	std::vector<int> dataSize;
	std::vector<char*> data;

	int getPrev(int prev);
	int getHead();
	int getTail();

	void lock();
	void unlock();

	void countup(int& val);
	void incTail();
	void incHead();
	void init(int ring_size);
	bool full();
	bool empty();

	RingBuffer();
	RingBuffer(int ring_size);
	~RingBuffer();
	void showDataSize();
};

class NetUDP
{
private:
	WSADATA wsaData;
	SOCKET sock;
	struct sockaddr_in sin_addr;
	struct sockaddr_in src_addr;
	void setSrcAddr(char* ip, int port);
	void setSinAddr(char* ip, int port);
	void setBind();

	HANDLE hThread;
	unsigned threadID;
	static unsigned __stdcall recvloopCaller(void* p);
protected:
	RingBuffer rbuffer;

public:
	bool isStop;
	//NET_UDP();
	NetUDP(char* src_ip, int src_port, char* sin_ip, int sin_port);
	~NetUDP();
	int send(char* str, const int send_size);
	int recv(char* str);

	void beginRecvThread();
};