#include "NetMat.h"

PacketHeader::PacketHeader(){;}
PacketHeader::PacketHeader(int64 sequence_number_, int64 timestamp_, uchar fragment_number_, uchar fragment_number_max_)
{
	sequence_number=sequence_number_;
	timestamp=timestamp_;
	fragment_number=fragment_number_;
	fragment_number_max=fragment_number_max_;
	for(int i=0;i<PACKET_USER_DEFINE_PARAMETER;i++) parameter[i]=0.f;
}
PacketHeader::PacketHeader(int64 sequence_number_, int64 timestamp_, uchar fragment_number_, uchar fragment_number_max_, float* param)
{
	sequence_number=sequence_number_;
	timestamp=timestamp_;
	fragment_number=fragment_number_;
	fragment_number_max=fragment_number_max_;
	for(int i=0;i<PACKET_USER_DEFINE_PARAMETER;i++) parameter[i]=param[i];
}
void PacketHeader::show()
{
	cout<<"Seq. "<<sequence_number<<", Frag. "<<(int)fragment_number+1<<"/"<<(int)fragment_number_max<<": time stamp "<<timestamp<<endl;
}


void NetMat::getPacketHeader(const char* src, PacketHeader& phead)
{
	phead.sequence_number = *(int64*)src;
	phead.timestamp = *(int64*)(src+8);
	phead.fragment_number = *(uchar*)(src+16);
	phead.fragment_number_max = *(uchar*)(src+17);

	for(int i=0;i<PACKET_USER_DEFINE_PARAMETER;i++)
	{
		phead.parameter[i] = *(float*)(src+18+i*4);
	}
}

int NetMat::makePacket(char* src , const int data_size, PacketHeader& phead)
{
	int64* sequence_num = (int64*)packet;
	int64* time_stamp = (int64*)(packet+8);
	uchar* fragment_num = (uchar*)(packet+16);
	uchar* fragment_num_max = (uchar*)(packet+17);
	float* info1 = (float*)(packet+18);
	float* info2 = (float*)(packet+22);

	*sequence_num = phead.sequence_number;
	*time_stamp = phead.timestamp;
	*fragment_num = phead.fragment_number;
	*fragment_num_max = phead.fragment_number_max;
	for(int i=0;i<PACKET_USER_DEFINE_PARAMETER;i++)
	{
		*(float*)(packet+18+i*4) = phead.parameter[i];
	}
	memcpy(packet+NET_OPENCV_HEADERSIZE,src, data_size);

	return data_size+NET_OPENCV_HEADERSIZE;
}

int NetMat::fragmented_send(vector<uchar>& src, const int64 timestamp)
{
	int total_sended = 0;
	char* sendbuff = (char*)&src[0];
	const int sending_size = (int)src.size();

	const int fragment_num_max = sending_size/(NET_UDP_SEND_MAX-NET_OPENCV_HEADERSIZE) + 1;
	for(int i=0;i<fragment_num_max;i++)
	{
		int data_size;

		if(i==fragment_num_max-1) data_size = sending_size -(NET_UDP_SEND_MAX-NET_OPENCV_HEADERSIZE)*(fragment_num_max-1);
		else data_size = NET_UDP_SEND_MAX-NET_OPENCV_HEADERSIZE;

		PacketHeader phead(sequence_number,timestamp,(uchar)i,fragment_num_max);
		int ssize = makePacket(sendbuff + (NET_UDP_SEND_MAX-NET_OPENCV_HEADERSIZE)*i, data_size, phead);

		total_sended+= send(packet,ssize);
		//Sleep(1.0);
	}
	sequence_number++;
	return total_sended;
}

bool NetMat::findIndex(int64& seqindex, int& fragment_max)
{
	bool isFind = false;
	PacketHeader phead;
	for(int i=0;i<rbuffer.size;i++)
	{
		int count=0;
		int cur = rbuffer.getHead();
		for(int i=0;i<rbuffer.size;i++)
		{
			getPacketHeader(rbuffer.data[cur],phead);
			if(phead.sequence_number==seqindex)
			{
				count++;
				if(phead.fragment_number_max==count)
				{
					isFind=true;
					fragment_max = phead.fragment_number_max;
				}
			}
			cur = rbuffer.getPrev(cur);
		}
		if(isFind==true)break;
		seqindex--;
	}
	return isFind;
}

//return size of data
int NetMat::grabIndex(vector<uchar>& dest, const int64 index , int64& sequenceIndex, int mode, int waitLoopMax)
{
	int total_recved = 0;	
	PacketHeader phead;
	int cur;
	char* dataTemp=NULL;

	int64 seqindex;
	int fragment_max;
	bool isFind;

	for(int i=0;i<waitLoopMax;i++)
	{
		seqindex =index;
		fragment_max=-1000;
		isFind = false;

		rbuffer.lock();//critical section

		isFind = findIndex(seqindex,fragment_max);

		if(seqindex !=index)
		{
			rbuffer.unlock();
			//cout<<"not found"<<endl;
		}
		else break;

		Sleep(1);
	}

	if(isFind)
	{
		dataTemp = new char[NET_UDP_SEND_MAX*fragment_max];
		cur = rbuffer.getHead();
		for(int i=0;i<rbuffer.size;i++)
		{
			getPacketHeader(rbuffer.data[cur],phead);
			sequenceIndex=seqindex;
			if(phead.sequence_number==sequenceIndex)
			{
				memcpy(dataTemp +phead.fragment_number*(NET_UDP_SEND_MAX-NET_OPENCV_HEADERSIZE),
					rbuffer.data[cur]+NET_OPENCV_HEADERSIZE,sizeof(char)*(rbuffer.dataSize[cur]-NET_OPENCV_HEADERSIZE));

				total_recved +=rbuffer.dataSize[cur];
			}
			cur = rbuffer.getPrev(cur);
		}

		const int dsize = total_recved - NET_OPENCV_HEADERSIZE*phead.fragment_number_max;
		dest.resize(dsize);
		prev.resize(dsize);
		uchar* dst = &dest[0];
		memcpy(dst,dataTemp,sizeof(char)*dsize);
		uchar* pdst = &prev[0];
		memcpy(pdst,dataTemp,sizeof(char)*dsize);
	}
	else 
	{
		if(mode == ELSE_GRAB_PREV_OUTPUT)
		{
			const int dsize = (int)prev.size();

			dest.resize(dsize);
			uchar* dst = &dest[0];
			uchar* pdst = &prev[0];
			memcpy(dst,pdst,sizeof(char)*dsize);
		}
		else if (mode == ELSE_GRAB_HEAD)
		{
			cout<<"call head"<<endl;

			sequenceIndex = getHeadIndex();
			findIndex(sequenceIndex, fragment_max);

			dataTemp = new char[NET_UDP_SEND_MAX*fragment_max];
			cur = rbuffer.getHead();
			for(int i=0;i<rbuffer.size;i++)
			{
				getPacketHeader(rbuffer.data[cur],phead);
				if(phead.sequence_number==sequenceIndex)
				{
					memcpy(dataTemp +phead.fragment_number*(NET_UDP_SEND_MAX-NET_OPENCV_HEADERSIZE),
						rbuffer.data[cur]+NET_OPENCV_HEADERSIZE,sizeof(char)*(rbuffer.dataSize[cur]-NET_OPENCV_HEADERSIZE));
					//cout<<"size :"<<rbuffer.dataSize[cur]<<endl;
					total_recved +=rbuffer.dataSize[cur];
				}
				cur = rbuffer.getPrev(cur);
			}
			const int dsize = total_recved - NET_OPENCV_HEADERSIZE*phead.fragment_number_max;

			dest.resize(dsize);
			prev.resize(dsize);
			uchar* dst = &dest[0];
			memcpy(dst,dataTemp,sizeof(char)*dsize);
			uchar* pdst = &prev[0];
			memcpy(pdst,dataTemp,sizeof(char)*dsize);
		}
	}

	rbuffer.unlock();
	delete[] dataTemp;
	return total_recved;
}

int64 NetMat::getHeadIndex()
{
	int64 seq_num_max=0;
	PacketHeader phead;
	int cur = rbuffer.getHead();

	for(int i=0;i<rbuffer.size;i++)
	{
		getPacketHeader(rbuffer.data[cur],phead);
		if(seq_num_max<phead.sequence_number)
		{
			seq_num_max = phead.sequence_number;
		}
		cur = rbuffer.getPrev(cur);
	}
	return seq_num_max;

}

//return recv size
int NetMat::grabHead(vector<uchar>& dest, int64& sequenceIndex)
{
	int total_recved = 0;
	PacketHeader phead;
	int cur;

	int64 seqindex=0;
	char* dataTemp=NULL;

	rbuffer.lock();//critical section

	cur = rbuffer.getHead();
	int64 seq_num_max=0;
	bool isFragment = false;
	for(int i=0;i<rbuffer.size;i++)
	{
		getPacketHeader(rbuffer.data[cur],phead);
		if(seq_num_max<phead.sequence_number)
		{
			seq_num_max = phead.sequence_number;
			if(phead.fragment_number_max>1)isFragment=true;
			else isFragment=false;
		}
		cur = rbuffer.getPrev(cur);
	}

	//cout<<seq_num_max<<endl;
	//if(isFragment) seqindex = seq_num_max-(max((int)lastOutputIndex,1));
	//	else seqindex =seq_num_max-(lastOutputIndex);

	if(isFragment) seqindex = seq_num_max-1;
	else seqindex =seq_num_max;

	bool isFind = false;
	int fragment_max=-1000;
	for(int i=0;i<rbuffer.size;i++)
	{
		int count=0;
		cur = rbuffer.getHead();
		for(int i=0;i<rbuffer.size;i++)
		{
			getPacketHeader(rbuffer.data[cur],phead);
			if(phead.sequence_number==seqindex)
			{
				count++;
				if(phead.fragment_number_max==count)
				{
					isFind=true;
					fragment_max = phead.fragment_number_max;
				}
			}
			cur = rbuffer.getPrev(cur);
		}
		if(isFind==true)break;
		seqindex--;
	}

	if(isFind)
	{
		dataTemp = new char[NET_UDP_SEND_MAX*fragment_max];
		cur = rbuffer.getHead();
		sequenceIndex = seqindex;
		for(int i=0;i<rbuffer.size;i++)
		{
			getPacketHeader(rbuffer.data[cur],phead);
			if(phead.sequence_number==seqindex)
			{
				memcpy(dataTemp +phead.fragment_number*(NET_UDP_SEND_MAX-NET_OPENCV_HEADERSIZE),
					rbuffer.data[cur]+NET_OPENCV_HEADERSIZE,sizeof(char)*(rbuffer.dataSize[cur]-NET_OPENCV_HEADERSIZE));
				//cout<<"size :"<<rbuffer.dataSize[cur]<<endl;
				total_recved +=rbuffer.dataSize[cur];
			}
			cur = rbuffer.getPrev(cur);
		}
		const int dsize = total_recved - NET_OPENCV_HEADERSIZE*phead.fragment_number_max;

		dest.resize(dsize);
		prev.resize(dsize);
		uchar* dst = &dest[0];
		memcpy(dst,dataTemp,sizeof(char)*dsize);
		uchar* pdst = &prev[0];
		memcpy(pdst,dataTemp,sizeof(char)*dsize);
	}
	else
	{
		//cout<<"no data"<<endl;
		const int dsize = (int)prev.size();

		dest.resize(dsize);
		uchar* dst = &dest[0];
		uchar* pdst = &prev[0];
		memcpy(dst,pdst,sizeof(char)*dsize);
	}

	rbuffer.unlock();
	delete[] dataTemp;
	return total_recved;
}

int NetMat::fragmented_recv(vector<uchar>& dest)
{
	char* recvbuff = new char[NET_UDP_SEND_MAX];

	int total_recved = recv(recvbuff);

	PacketHeader phead;
	getPacketHeader(recvbuff,phead);

	while(phead.fragment_number!=0)
	{
		total_recved = recv(recvbuff);
		getPacketHeader(recvbuff,phead);
	}



	char* dataTemp = new char[NET_UDP_SEND_MAX*phead.fragment_number_max];

	memcpy(dataTemp +phead.fragment_number*(NET_UDP_SEND_MAX-NET_OPENCV_HEADERSIZE) ,recvbuff+NET_OPENCV_HEADERSIZE,sizeof(char)*(total_recved-NET_OPENCV_HEADERSIZE));

	const int64 sequenceNum= phead.sequence_number;
	const int sequenceMax= (int)phead.fragment_number_max;
	int checksum = 1;
	for(int i=1;i<phead.fragment_number_max;i++)
	{
		int rcvsize = recv(recvbuff);
		getPacketHeader(recvbuff,phead); 
		if(phead.sequence_number == sequenceNum) checksum++;

		memcpy(dataTemp +phead.fragment_number*(NET_UDP_SEND_MAX-NET_OPENCV_HEADERSIZE),
			recvbuff+NET_OPENCV_HEADERSIZE,
			sizeof(char)*(rcvsize-NET_OPENCV_HEADERSIZE));
		total_recved+=rcvsize;	
	}

	if(checksum==sequenceMax)
	{
		const int dsize = total_recved - NET_OPENCV_HEADERSIZE*phead.fragment_number_max;
		dest.resize(dsize);
		prev.resize(dsize);
		uchar* dst = &dest[0];
		memcpy(dst,dataTemp,sizeof(char)*dsize);
		uchar* pdst = &prev[0];
		memcpy(pdst,dataTemp,sizeof(char)*dsize);
	}
	else
	{
		const int dsize = (int)prev.size();

		dest.resize(dsize);
		uchar* dst = &dest[0];
		uchar* pdst = &prev[0];
		memcpy(dst,pdst,sizeof(char)*dsize);
	}

	delete[] recvbuff;
	delete[] dataTemp;
	return total_recved;
}


void NetMat::initRingBuffer()
{
	for(int i=0; i<rbuffer.size; i++)
	{
		PacketHeader phead(LONG_MAX,LONG_MAX,0,255);
		char a=0;
		int s = makePacket(&a,1,phead);
		memcpy(rbuffer.data[i],packet,s);
	}
}

NetMat::NetMat(char* src_ip, int src_port, char* sin_ip, int sin_port) : NetUDP(src_ip, src_port, sin_ip, sin_port)
{
	prev_sequence_number=0;
	sequence_number=0;
	packet = new char[USHRT_MAX];
	initRingBuffer();
}

NetMat::~NetMat()
{
	delete[] packet;
	this->isStop = true;
}

void NetMat::waitReady()
{
	std::cout<<"Waiting..."<<std::endl;
	bool ready = false;

	while(!ready)
	{
		ready = true;
		int size = rbuffer.size;
		for(int i=0; i<rbuffer.size; i++)
		{
			if(rbuffer.dataSize[i]<=0)
			{
				size--;
				ready = false;
			}
		}
		if(!ready)Sleep(1000);
		cout<<size/(double)(rbuffer.size)*100<<"%"<<endl;
	}
}

int NetMat::sendMat(const Mat& src, int compression, int parameter)
{
	const int64 timestamp = cv::getTickCount();
	std::vector<uchar> buffer1;

	int pngparam = parameter > 10 ? parameter/10 :parameter;
	if(compression==CV_IMWRITE_JPEG_QUALITY)
	{
		/*vector<int> param(2);
		param[0]=CV_IMWRITE_JPEG_QUALITY;
		param[1]=parameter;
		imencode(".jpg",src, buff, param);*/

		//uchar* a = new uchar[src.size().area()*src.channels()];
		//imencodeJPEG(src, buffer1,parameter,DCT_IFAST,isOpt);
		vector<int> param(2);
		param[0]=CV_IMWRITE_JPEG_QUALITY;
		param[1]=parameter;
		//int s = imencodeJPEG(src, a,parameter,DCT_IFAST,isOpt);
		int s = imencode(".jpg",src, buffer1,param);
		//buffer1.resize(s);
		//memcpy(&buffer1[0],a,s);
		//delete[] a;
	}

	else if(compression==CV_IMWRITE_PNG_COMPRESSION)
	{
		vector<int> param(4);
		param[0]=CV_IMWRITE_PNG_COMPRESSION;
		param[1]=std::max(1,std::min(parameter,9));
		param[2]=CV_IMWRITE_PNG_STRATEGY;
		if(pngparam == 0)
			param[3]=CV_IMWRITE_PNG_STRATEGY_DEFAULT;
		else if (pngparam == 1)
			param[3]=CV_IMWRITE_PNG_STRATEGY_FILTERED;
		else if (pngparam == 2)
			param[3]=CV_IMWRITE_PNG_STRATEGY_FIXED;
		else if (pngparam == 3)
			param[3]=CV_IMWRITE_PNG_STRATEGY_HUFFMAN_ONLY;
		else if (pngparam == 4)
			param[3]=CV_IMWRITE_PNG_STRATEGY_RLE;

		imencode(".png",src, buffer1, param);
	}

	int sended_size = 0;
	sended_size = fragmented_send( buffer1,timestamp);

	return sended_size;
}

int64 NetMat::recvMatIndex(Mat& dest, int64 index, int flags)
{
	int64 sequenceIndex;
	vector<uchar> buff;

	grabIndex(buff,index, sequenceIndex, ELSE_GRAB_HEAD);

	Mat tmp = imdecode(buff,flags);

	if(!tmp.empty())
	{
		tmp.copyTo(dest);
		prev_sequence_number = sequenceIndex;
	}

	return sequenceIndex;
}

int64 NetMat::recvMatNext(Mat& dest, int flags)
{
	int64 index = prev_sequence_number+1;
	return recvMatIndex(dest, index, flags);
}

int64 NetMat::recvMatHead(Mat& dest,int flags)
{
	int64 sequenceIndex;
	vector<uchar> buff;
	int recv_size = grabHead(buff, sequenceIndex);

	Mat tmp = imdecode(buff,flags);
	if(!tmp.empty())
	{
		tmp.copyTo(dest);
		prev_sequence_number = sequenceIndex;
	}

	return sequenceIndex;
}