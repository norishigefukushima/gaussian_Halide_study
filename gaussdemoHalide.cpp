// gaussdemoHalide.cpp : アプリケーションのエントリ ポイントを定義します。
//

#include "stdafx.h"


#include <opencv2/opencv.hpp>
#include "Halide.h"
#include "util.h"
#include "filterAVX.h"
using namespace cv;
using namespace std;
using namespace Halide;

#pragma comment (lib, "opencv_core331.lib")
#pragma comment (lib, "opencv_highgui331.lib")
#pragma comment (lib, "opencv_imgcodecs331.lib")
#pragma comment (lib, "opencv_imgproc331.lib")

#pragma comment(lib, "Halide.lib")

class blur2DSeparableHalide
{
public:
	Halide::Buffer<float> input;
	int radius;
	float sigma;
	int xy;
	int method;
	Func apply;
	int schedule;
	string schedulename;
	int tilex;
	int tiley;
	int vectorl;

	blur2DSeparableHalide()
	{
		xy = 0;
		method = 0;
		schedule = 0;
		tilex = 64;
		tiley = 32;
		vectorl = 32;
		schedulename = "compute vec para";
	}

	enum
	{
			COMPUTE_INLINE = 0,
			REORDER_YX,
			COMPUTE_ROOT_BORDER,
			COMPUTE_ROOT_FIRSTFILTER,
			COMPUTE_ROOT_FIRSTFILTER2,
			COMPUTE_ROOT_BORDER_TILE,
			COMPUTE_ROOT_BORDER_TILE2,
			COMPUTE_AT_TILE,
			COMPUTE_AT_STOTE_TILE,
			COMPUTE_ROOT_BORDER_AT_TILE,
			COMPUTE_ROOT_BORDER_AT_STOTE_TILE,
	};
	void generate(int xyorder, int smethod)
	{
		xy = xyorder;

		Expr D = 2 * radius + 1;
		RDom r(-radius, D, "r");

		Func inputimg = BoundaryConditions::mirror_interior(input);
		Var wx("wx"), x("x"), y("y");
		Var  xi("xi"), yi("yi");

		const float d = -1.f / (2.f*sigma*sigma);
		float  total = 0.f;
		for (int j = 0; j < 2 * radius + 1; j++)
		{
			total += std::exp(((float)(j - radius)*(j - radius))*d);
		}
		total = 1.f / total;
		Func weight;
		weight(wx) = fast_exp((wx*wx)*d)*total;
		weight.compute_root();

		Func blur1, blur2, buff;
		if (smethod == COMPUTE_ROOT_BORDER_TILE2 ||
			smethod == COMPUTE_ROOT_FIRSTFILTER2)
		{
			if (xyorder == 0)
			{
				blur1(x, y) = sum(weight(r)*inputimg(x + r, y));
				buff(x, y) = blur1(x, y);
				blur2(x, y) = sum(weight(r)*buff(x, y + r));
			}
			else if (xyorder == 1)
			{
				blur1(x, y) = sum(weight(r)*inputimg(x, y + r));
				buff(x, y) = blur1(x, y);
				blur2(x, y) = sum(weight(r)*buff(x + r, y));
			}
		}
		else
		{
			if (xyorder == 0)
			{
				blur1(x, y) = sum(weight(r)*inputimg(x + r, y));
				blur2(x, y) = sum(weight(r)*blur1(x, y + r));
			}
			else if (xyorder == 1)
			{
				blur1(x, y) = sum(weight(r)*inputimg(x, y + r));
				blur2(x, y) = sum(weight(r)*blur1(x + r, y));
			}
		}

		//schedule
		
		if (smethod == COMPUTE_INLINE)
		{
			blur2.vectorize(x, vectorl).parallel(y);
			schedulename = "COMPUTE_INLINE";
		}
		else if (smethod == REORDER_YX)
		{
			blur2.reorder(y, x).vectorize(x, vectorl).parallel(y);
			schedulename = "REORDER_YX";
		}
		else if (smethod == COMPUTE_ROOT_BORDER)
		{
			inputimg.compute_root();
			blur2.vectorize(x, vectorl).parallel(y);
			schedulename = "COMPUTE_ROOT_BORDER";
		}
		else if (smethod == COMPUTE_ROOT_FIRSTFILTER)
		{
			blur1.compute_root().vectorize(x, vectorl).parallel(y);
			blur2.vectorize(x, vectorl).parallel(y);
			schedulename = "COMPUTE_FIRSTFILTER";
		}
		else if (smethod == COMPUTE_ROOT_FIRSTFILTER2)
		{
			blur1.compute_root().vectorize(x, vectorl).parallel(y);
			buff.compute_root();
			blur2.vectorize(x, vectorl).parallel(y);
			schedulename = "COMPUTE_FIRSTFILTER";
		}
		else if (smethod == COMPUTE_ROOT_BORDER_TILE)
		{
			inputimg.compute_root();
			blur1.compute_root().vectorize(x, vectorl).parallel(y);
			blur2.vectorize(x, vectorl).parallel(y);
			schedulename = "COMPUTE_ROOT_BORDER_TILE";
		}
		else if (smethod == COMPUTE_ROOT_BORDER_TILE2)
		{
			inputimg.compute_root();
			blur1.compute_root().vectorize(x, vectorl).parallel(y);
			buff.compute_root();
			blur2.vectorize(x, vectorl).parallel(y);
			schedulename = "COMPUTE_ROOT_BORDER_TILE2";
		}
		else if (smethod == COMPUTE_AT_TILE)
		{
			blur2.tile(x, y, x, y, xi, yi, tilex, tiley).vectorize(xi, vectorl).parallel(y);
			blur1.compute_at(blur2, x).vectorize(x, vectorl);
			schedulename = "COMPUTE_AT_TILE";
		}
		else if (smethod == COMPUTE_AT_STOTE_TILE)
		{
			blur2.tile(x, y, x, y, xi, yi, tilex, tiley).vectorize(xi, vectorl).parallel(y);
			blur1.compute_at(blur2, x).store_at(blur2, x).vectorize(x, vectorl);
			schedulename = "COMPUTE_AT_STOTE_TILE";
		}
		else if (smethod == COMPUTE_ROOT_BORDER_AT_TILE)
		{
			inputimg.compute_root();
			blur2.tile(x, y, x, y, xi, yi, tilex, tiley).vectorize(xi, vectorl).parallel(y);
			blur1.compute_at(blur2, x).vectorize(x, vectorl);
			schedulename = "COMPUTE_ROOT_BORDER_AT_TILE";
		}
		else if (smethod == COMPUTE_ROOT_BORDER_AT_STOTE_TILE)
		{
			inputimg.compute_root();
			blur2.tile(x, y, x, y, xi, yi, tilex, tiley).vectorize(xi, vectorl).parallel(y);
			blur1.compute_at(blur2, x).store_at(blur2, x).vectorize(x, vectorl);
			schedulename = "COMPUTE_ROOT_BORDER_AT_STOTE_TILE";
		}
		else if (smethod == 100)
		{
			schedulename = "";
		}

		apply = blur2;
	}

	void setParameter(const Halide::Buffer<float>& in, const int r, const float sigma_)
	{
		input = in;
		radius = r;
		sigma = sigma_;
		generate(xy, schedule);
	}
};

void blur2DTest()
{
	string wname = "Halide";
	namedWindow(wname);
	int r = 3; createTrackbar("r", "", &r, 100);
	int sw = 3; createTrackbar("sw", "", &sw, 4);
	int schedule = 0; createTrackbar("schedule", "", &schedule, 12);
	int tilex = 5;  createTrackbar("tilex", "", &tilex, 10);
	int tiley = 5;  createTrackbar("tiley", "", &tiley, 10);
	int vectorl = 5; createTrackbar("vectorl", "", &vectorl, 7);

	Mat src_ = imread("lenna.png", 0);
	Mat src;
	//resize(src_, src, Size(), 16, 8);
	resize(src_, src, Size(), 1, 1);
	Mat blur = Mat::zeros(src.size(), src.type());
	Mat src32f, blur32f, input32f;
	src.convertTo(src32f, CV_32F);
	src.convertTo(input32f, CV_32F);
	blur.convertTo(blur32f, CV_32F);
	Mat rnd(src32f.size(), CV_32F);
	Halide::Buffer<float> input(input32f.ptr<float>(0), src.cols, src.rows);
	Halide::Buffer<float> output(blur32f.ptr<float>(0), blur.cols, blur.rows);

	blur2DSeparableAVX fsep;
	blur2DAVX f;
	blur2DSeparableHalide halidef;

	ConsoleImage ci;
	halidef.setParameter(input, r, r / 3.f);


	int64 start;
	double time;
	double tmul = 1000.0 / cv::getTickFrequency();
	int key = 0;
	Stat st;
	Stat sto;
	while (key != 'q')
	{
		randu(rnd, 0, 20);
		//add(src32f, rnd, input32f);
		src32f.copyTo(input32f);
		blur32f.setTo(0);

		Mat ref;
		f.filter(input32f, blur32f, r, r / 3.f);
		blur32f.convertTo(ref, CV_8U);

		start = cv::getTickCount();
		GaussianBlur(input32f, blur32f, Size(2 * r + 1, 2 * r + 1), r / 3.f);
		blur32f.convertTo(blur, CV_8U);
		double opencvtime = (cv::getTickCount() - start) * tmul;
		sto.push_back(opencvtime);
		start = cv::getTickCount();
		string mes;

		if (sw == 0)
		{
			GaussianBlur(input32f, blur32f, Size(2 * r + 1, 2 * r + 1), r / 3.f);
			blur32f.convertTo(blur, CV_8U);
			mes = "OpenCV: ";
		}
		else if (sw == 1)
		{
			f.filter(input32f, blur32f, r, r / 3.f);
			blur32f.convertTo(blur, CV_8U);
			mes = "AVX-OMP: ";
		}
		else if (sw == 2)
		{
			start = cv::getTickCount();
			fsep.filter(input32f, blur32f, r, r / 3.f);
			blur32f.convertTo(blur, CV_8U);
			mes = "AVX-OMP Sep: ";
		}
		else if (sw == 3)
		{
			start = cv::getTickCount();
			if (halidef.radius != r || halidef.xy != 0 || halidef.schedule != schedule ||
				halidef.tilex != pow(2, tilex) || halidef.tiley != pow(2, tiley) || halidef.vectorl != pow(2, vectorl)
				)
			{
				halidef.xy = 0;
				halidef.tilex = pow(2, tilex);
				halidef.tiley = pow(2, tiley);
				halidef.vectorl = pow(2, vectorl);

				halidef.schedule = schedule;
				halidef.setParameter(input, r, r / 3.f);
			}
			halidef.apply.realize(output);
			blur32f.convertTo(blur, CV_8U);
			mes = "Halide BorderXY: ";
		}
		else if (sw == 4)
		{
			start = cv::getTickCount();
			if (halidef.radius != r || halidef.xy != 1 || halidef.schedule != schedule ||
				halidef.tilex != pow(2, tilex) || halidef.tiley != pow(2, tiley) || halidef.vectorl != pow(2, vectorl))
			{
				halidef.xy = 1;
				halidef.tilex = pow(2, tilex);
				halidef.tiley = pow(2, tiley);
				halidef.vectorl = pow(2, vectorl);

				halidef.schedule = schedule;
				halidef.setParameter(input, r, r / 3.f);
			}
			halidef.apply.realize(output);
			blur32f.convertTo(blur, CV_8U);
			mes = "Halide BorderYX: ";
		}

		time = (cv::getTickCount() - start) * tmul;
		st.push_back(time);

		ci(mes);
		ci(format("Time(curr): %f ms", time));
		ci(format("Time(med) : %f ms", st.getMedian()));
		ci(format("Time(mean): %f ms", st.getMean()));
		ci(format("Time(min ): %f ms", st.getMin()));
		ci(format("vsOpenCV  : x%f", sto.getMin()/ st.getMin()));
		
		ci(format("PSNR: %f dB", PSNR(ref, blur)));

		ci(format("TileXY: %d %d", halidef.tilex, halidef.tiley));
		ci(format("vector: %d ", halidef.vectorl));
		ci(halidef.schedulename);
		//ci(format("TileXY: %d %d", (int)pow(2, tilex), (int)pow(2, tiley)));
		//ci(format("vector: %d ", (int)pow(2, vectorl)));

		imshow("console", ci.show);
		ci.flush();
		imshow(wname, blur);
		key = waitKey(1);
		if (key == 'r')
		{
			st.clear();
			sto.clear();
		}
	}
}

int main(int argc, char **argv)
{
	blur2DTest();
	return 0;
}

