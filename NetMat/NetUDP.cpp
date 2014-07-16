#include "NetUDP.h"

#include <string.h>
#include <iostream>
#include <process.h>

using namespace std;


int RingBuffer::getPrev(int prev)
{ 
	int val = prev-1;
	val = (val<0) ? size-1 :val;
	return val;
}
int RingBuffer::getHead(){return head;}

int RingBuffer::getTail(){return tail;}

void RingBuffer::lock()
{
	EnterCriticalSection(&section);
}
void RingBuffer::unlock()
{
	LeaveCriticalSection(&section);
}
void RingBuffer::countup(int& val)
{
	val++;
	val = val%size;
}
void RingBuffer::incTail()
{
	countup(tail);
}
void RingBuffer::incHead()
{
	countup(head);
}
bool RingBuffer::full()
{
	if(head == (tail+1)%size)true;
	else false;
}
bool RingBuffer::empty()
{
	if(head==tail)return true;
	else false;
}

void RingBuffer::init(int ring_size)
{
	isInit = true;
	delete[] dataPtr;
	head=0;
	tail=0;
	size = max(ring_size,1);
	dataSize.resize(size);
	data.resize(size);
	dataPtr = new char [size*USHRT_MAX];
	for(int i=0;i<size;i++)
	{
		dataSize[i]=0;
		data[i]=dataPtr + USHRT_MAX*i;
	}
}

RingBuffer::RingBuffer()
{
	isInit = false;
	InitializeCriticalSection(&section);
}
RingBuffer::RingBuffer(int ring_size)
{
	isInit = false;
	InitializeCriticalSection(&section);
	init(ring_size);
}
RingBuffer::~RingBuffer()
{
	DeleteCriticalSection(&section);
	delete[] dataPtr;
}
void RingBuffer::showDataSize()
{
	for(int i=0;i<size;i++)
		cout<<i<<":"<<dataSize[i]<<endl;
}


NetUDP :: NetUDP(char* src_ip, int src_port, char* sin_ip, int sin_port)
{
	if(WSAStartup(MAKEWORD(2, 0), &wsaData) != 0)
		exit(1);
	if((sock = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET){
		exit(1);
	}
	isStop=false;
	rbuffer.init(RING_BUFFER_SIZE);

	setSrcAddr(src_ip, src_port);
	setSinAddr(sin_ip, sin_port);
	setBind();
}

NetUDP :: ~NetUDP()
{
	WaitForSingleObject( hThread, INFINITE );
	CloseHandle( hThread );

	shutdown(sock, SD_BOTH);
	closesocket(sock);
	WSACleanup();
}

void NetUDP :: setSrcAddr(char* ip, int port)
{
	src_addr.sin_family = AF_INET;
	src_addr.sin_port = htons(port);
	if(inet_addr(ip) == -1)
		exit(1);
	src_addr.sin_addr.S_un.S_addr = inet_addr(ip);//INADDR_ANY;
	isStop=false;
}

void NetUDP :: setSinAddr(char* ip, int port)
{
	sin_addr.sin_family = AF_INET;
	sin_addr.sin_port = htons(port);
	if(inet_addr(ip) == -1)
		exit(1);
	sin_addr.sin_addr.S_un.S_addr = inet_addr(ip);//INADDR_ANY;
}

void NetUDP :: setBind()
{
	int a = bind(sock, (struct sockaddr *)&src_addr, sizeof(src_addr));
}

int NetUDP :: send(char* str, const int send_size)
{
	if(send_size > NET_UDP_SEND_MAX) fprintf(stderr,"send data overflow (sending size %d > sending max %d) \n",send_size,NET_UDP_SEND_MAX);

	int sended_size = sendto(sock, str, send_size, 0, (struct sockaddr *)&sin_addr, sizeof(sin_addr));

	if(sended_size == SOCKET_ERROR) fprintf(stderr,"send error\n");

	return sended_size;
}

int NetUDP :: recv(char* dest)
{	
	struct sockaddr_in clt;
	int sin_size = sizeof(struct sockaddr_in);
	int recv_size = recvfrom(sock, dest, sizeof(char)*USHRT_MAX, 0,  (struct sockaddr *)&clt, &sin_size);
	return recv_size;
}


unsigned __stdcall NetUDP :: recvloopCaller(void* p)
{	
	NetUDP* precv = (NetUDP*)p;
	char* recvbuff = new char[USHRT_MAX];
	while(!precv->isStop)
	{
		int recv_size = precv->recv(recvbuff);

		precv->rbuffer.lock();

		precv->rbuffer.dataSize[precv->rbuffer.getHead()]=recv_size;
		memcpy(precv->rbuffer.data[precv->rbuffer.getHead()],recvbuff, sizeof(char)*recv_size);
		precv->rbuffer.incHead();

		precv->rbuffer.unlock();
	}
	delete[]recvbuff;
	_endthreadex( 0 );
	return 0;
}

void NetUDP :: beginRecvThread()
{
	// Create the second thread.
	hThread = (HANDLE)_beginthreadex(NULL,0, &NetUDP::recvloopCaller, (LPVOID) this,0,NULL );
}