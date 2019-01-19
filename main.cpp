#include <iostream>

#include "opencv2/imgproc.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/opencv.hpp"
#include "ntcore/ntcore_cpp.h"
#include "ntcore/networktables/NetworkTable.h"
#include "ntcore/networktables/NetworkTableEntry.h"
#include "ntcore/networktables/NetworkTableInstance.h"


int main(){
	bool DEBUG = true;           //Do we want to show the output screams?
	bool NETWORK_TABLES = false; //Do we want to send values over Network Tables?

	std::cout << "Hello World!\n"; //Quick Test Message
	
	if(NETWORK_TABLES){ //Set up Network Tables stuff
		auto inst = nt::NetworkTableInstance::GetDefault();
		auto table = inst.GetTable("vision_table");
	
		nt::NetworkTableEntry testEntry = table->GetEntry("TestEntry");
	}

	//Camera Setup
	cv::VideoCapture cap(0);
	//cap.set(15, -6);
	if(!cap.isOpened()){
		return -1; //Make sure we can open the stream. If not, there is a problem.
	}

	for(;;){ //Infinite Processing Loop
		cv::Mat currentFrame;
		cv::Mat HSVFrame;
		cv::Mat ThresholdFrame;
		cv::Mat contourFrame;
		cv::Mat outputImage;

		cap >> currentFrame; //get a new frame
		
		cv::cvtColor(currentFrame, HSVFrame, CV_BGR2HSV);

		//HSV Thresholding
		cv::Scalar lower_bound = cv::Scalar(63, 89, 147);
		cv::Scalar upper_bound = cv::Scalar(91, 255, 255);
		
		cv::inRange(HSVFrame, lower_bound, upper_bound, ThresholdFrame);

		std::vector<std::vector<cv::Point> > contours;
		cv::findContours(ThresholdFrame, contours, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);


		int numContours = contours.size;
		int area1 = 1;
		int area2 = 0;

		if(numContours > 1){
			std::vector<cv::Point> fst_contour;
			std::vector<cv::Point> snd_contour;

			for(int i = 0; i < numContours; i++){
				int area = (int) cv::contourArea(conoturs[i]);

				if(area > area1){
					area2 = area1;
					snd_contour = fst_contour;
					fst_contour= contours[i];
					area1 = area;
				} else if(area > area2){
					snd_contour = contours[i];
					area2 = area;
				}
			}
		}
		

		if(DEBUG){
			imshow("Camera", currentFrame);
			imshow("HSV_Output", HSVFrame);
			imshow("Threshold Output", ThresholdFrame);
		}

		if(cv::waitKey(30) >= 0){
			break;
		}
	}
	/**
	for(;;){
		testEntry.setDouble(5332.0);
	}**/
}
