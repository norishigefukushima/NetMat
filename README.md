NetMat
======

Communication class of OpenCV Mat class  over UDP for Windows.

The class use UDP for real-time communication.Unlike using TCP/IP, UDP comunication contains packet-loss, flickers of oerdering of packets.  
The class of NetMat can recover these problem, and can output buffers with low latency.


Demo
====
call the file (NetMat.exe) for demo  
* recver side  
  NetMat.exe  
* sender side  
  NetMat.exe 1  
  
  
Class: NetMat 
=============
  

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
