#include "NetMat.h"

#define CV_VERSION_NUMBER CVAUX_STR(CV_MAJOR_VERSION) CVAUX_STR(CV_MINOR_VERSION) CVAUX_STR(CV_SUBMINOR_VERSION)

#ifdef _DEBUG
#pragma comment(lib, "opencv_imgproc"CV_VERSION_NUMBER"d.lib")
#pragma comment(lib, "opencv_highgui"CV_VERSION_NUMBER"d.lib")
#pragma comment(lib, "opencv_core"CV_VERSION_NUMBER"d.lib")
#else
#pragma comment(lib, "opencv_imgproc"CV_VERSION_NUMBER".lib")
#pragma comment(lib, "opencv_highgui"CV_VERSION_NUMBER".lib")
#pragma comment(lib, "opencv_core"CV_VERSION_NUMBER".lib")
#endif

void recver()
{	
	string wname = "recver";
	namedWindow(wname); 

	NetMat netimg("127.0.0.1", 2525, "127.0.0.1", 2526);

	netimg.beginRecvThread();//start recv-thread.
	netimg.waitReady();//waiting...
	
	Mat image;//recv iamge
	
	int tcount=0;
	int count=0;
	int64 preidx = 0;

	int key=0;
	while(key!='q')
	{
		int64 pre = getTickCount();

		//int64 idx = netimg.recvMatHead(image);// minimizing letency, but a lot of frames are discarded.
		int64 idx = netimg.recvMatNext(image);// grab next frame
		
		if(idx-preidx>0)tcount++;
		count++;

		preidx=idx;

		putText(image,format("recv: Seq. %d rate: %0.2f",(int)idx,100.0*tcount/(double)count),Point(20,50),CV_FONT_HERSHEY_DUPLEX,1.0,CV_RGB(255,255,255));
		
		imshow(wname,image);
		key = waitKey(1);	

		cout<<"time "<<(getTickCount()-pre)/(getTickFrequency())*1000<<" ms"<<endl;
	}
}

Mat generateImage(Mat& src, Size size, int index)
{
	Mat ret= Mat::zeros(size,CV_8UC3);
	int w = index%(src.cols-size.width);
	Point pt(w,100);	
	src(Rect(pt,size)).copyTo(ret);
	
	return ret;
}

void sender()
{
	NetMat netimg("127.0.0.1", 2526, "127.0.0.1", 2525);

	Mat simg = imread("pano.jpg");
	if(simg.empty())cout<<"invalid input image. please input image"<<endl;

	string wname = "sender";
	namedWindow(wname);

	//1:100 JPEG 101:104 png, 0:RAW(ppm)
	int q = 80; createTrackbar("q",wname,&q,104);

	int key = 0;
	int idx=0;

	double fps = 0.0;
	while(key!='q')
	{
		int64 pre = getTickCount();
		Mat show = generateImage(simg,Size(640,480),(int)netimg.sequence_number);

		int sendsizeimg=0;

		if(q>100)
			sendsizeimg = netimg.sendMat(show,CV_IMWRITE_PNG_COMPRESSION,q-100);
		else if (q==0)
			sendsizeimg = netimg.sendMat(show,CV_IMWRITE_PXM_BINARY,0);
		else
			sendsizeimg = netimg.sendMat(show,CV_IMWRITE_JPEG_QUALITY,q);


		double bpsimg = 8.0*sendsizeimg*30/1000000.0;


		if(q>100)
			putText(show,format("PNG: %.02f Mbps, fragment %02d",bpsimg,sendsizeimg/NET_UDP_SEND_MAX+1),Point(30,60),CV_FONT_HERSHEY_DUPLEX,1.0,CV_RGB(0,255,0));
		else
			putText(show,format("JPG: %.02f Mbps, fragment %02d",bpsimg,sendsizeimg/NET_UDP_SEND_MAX+1),Point(30,60),CV_FONT_HERSHEY_DUPLEX,1.0,CV_RGB(0,255,0));

		putText(show,format("No. %03d",(int)netimg.sequence_number-1),Point(30,100),CV_FONT_HERSHEY_DUPLEX,1.0,CV_RGB(255,255,255));

		imshow(wname,show);
		key = waitKey(1);
		double time = (getTickCount()-pre)/(getTickFrequency())*1000;
		cout<<"time "<<time<<" ms"<<endl;
		double fps = 1000.0/time;
	}
}


int main(int argc, char** argv)
{
	if(argc==1) recver();
	else sender();

	return 0;
}