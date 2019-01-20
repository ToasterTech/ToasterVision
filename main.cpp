#include <iostream>

#include "opencv2/imgproc.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/opencv.hpp"
#include "ntcore/ntcore_cpp.h"
#include "ntcore/networktables/NetworkTable.h"
#include "ntcore/networktables/NetworkTableEntry.h"
#include "ntcore/networktables/NetworkTableInstance.h"

//GLOBAL VARIABLES
//Debug Variables
bool DEBUG = true;           //Do we want to show the output screams?
bool NETWORK_TABLES = false; //Do we want to send values over Network Tables?

//Global Contour Variables
int cx1 = 0, cx2 = 0, cx = 0; //Center X points of Contour 1, Contour 2, and Center Point
int cy1 = 0, cy2 = 0, cy = 0; //The same thing but for y-coordinates
int area1, area2; //Two Largest Areas
std::vector<cv::Point> fst_contour, snd_contour; //Two Largest Contours

//Global Mat Objects
cv::Mat currentFrame; //Output of Camera
cv::Mat HSVFrame;     //HSV Conversion Output
cv::Mat ThresholdFrame; //Output of HSV Filtering
cv::Mat contourFrame;   //Draws All Contours
cv::Mat outputFrame;    //Draws Top Two Contours with Target Point

int main(){

	std::cout << "Hello World!\n"; //Quick Test Message
	
	if(NETWORK_TABLES){ //Set up Network Tables stuff
		auto inst = nt::NetworkTableInstance::GetDefault();
		auto table = inst.GetTable("vision_table");
	
		nt::NetworkTableEntry testEntry = table->GetEntry("TestEntry");
	}

	
	//Camera Setup
	
	cv::VideoCapture cap(1);

	if(!cap.isOpened()){
		return -1; //Make sure we can open the stream. If not, there is a problem.
	} else {
		system("v4l2-ctl -d /dev/video1 -c exposure_auto=1");
		system("v4l2-ctl -d /dev/video1 -c exposure_absolute=25");
	}
	//Uncomment the Line below if using a static image.
	//currentFrame = cv::imread("../PracticeImages/Image7.png", -1);
	
	contourFrame = cv::Mat::zeros(currentFrame.size(), CV_8UC3);

	for(;;){ //Infinite Processing Loop

		//if(VIDEO_STREAM) {cap >> currentFrame;} //get a new frame
		
		cap >> currentFrame;
		cv::cvtColor(currentFrame, HSVFrame, cv::COLOR_BGR2HSV);

		//HSV Thresholding
		cv::Scalar lower_bound = cv::Scalar(28, 28, 174);
		cv::Scalar upper_bound = cv::Scalar(105, 205, 255);
		
		cv::inRange(HSVFrame, lower_bound, upper_bound, ThresholdFrame);

		std::vector<std::vector<cv::Point>> contours;
		std::vector<std::vector<cv::Point>> rectangleContours;
		std::vector<std::vector<cv::Point>> filteredContours;

		cv::findContours(ThresholdFrame, contours, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);
		//std::cout << "Num of Contours (Pre-Filter)" << contours.size() << "\n";

		int numContours = contours.size();
		int area1 = 1;
		int area2 = 0;

		
		for(std::vector<cv::Point> contour : contours){
			approxPolyDP(contour, contour, 0.02*cv::arcLength(contour, true), true);
						
			
			if(contour.size() == 4){
				rectangleContours.push_back(contour);
			}

			std::cout << "Sides: " << contour.size() << "\n";
		}

		if(rectangleContours.size() >  1){

			for(int i = 0; i < rectangleContours.size(); i++){
				int area = (int) cv::contourArea(rectangleContours[i]);

				if(area > area1){
					area2 = area1;
					snd_contour = fst_contour;
					fst_contour= rectangleContours[i];
					area1 = area;
				} else if(area > area2){
					snd_contour = rectangleContours[i];
					area2 = area;
				}
			}
		} else { //There aren't enough contours
			std::cout << "Not enough Rectangles" << "\n";
		}

		filteredContours.push_back(fst_contour);
		filteredContours.push_back(snd_contour);
		
		cv::Moments m1 = cv::moments(fst_contour);
		cv::Moments m2 = cv::moments(snd_contour);
		

		if((m1.m00 != 0) && (m2.m00 != 0)){
			cx1 = (int) m1.m10 / m1.m00;
			cx2 = (int) m2.m10 / m2.m00;
			cx  = (int) (cx1 + cx2) / 2;
			
			cy1 = m1.m01 / m1.m00;
			cy2 = m2.m01 / m2.m00;
			cy  = (cy1 + cy2) / 2;

		}

		if(DEBUG){
			contourFrame = cv::Mat::zeros(currentFrame.size(), CV_8UC3);

			outputFrame = currentFrame.clone();

			if(contours.size() != 0){
				cv::drawContours(contourFrame, contours, -1, cv::Scalar(255, 191, 0), 2);
			}
			
			cv::circle(outputFrame, cv::Point(cx1, cy1), 10, cv::Scalar(0, 0, 255), 10);
			cv::circle(outputFrame, cv::Point(cx2, cy2), 10, cv::Scalar(0, 0, 255), 10);
			cv::circle(outputFrame, cv::Point(cx, cy), 10, cv::Scalar(0, 0, 255), 10);

			imshow("Camera", currentFrame);
			imshow("HSV_Output", HSVFrame);
			imshow("Threshold Output", ThresholdFrame);
			imshow("Contour Output", contourFrame);
			imshow("Final Output", outputFrame);
		}

		if(cv::waitKey(30) >= 0){
			break;
		}
	}
	/**
	for(;;){
		testEntry.setDouble(5332.0);
	}**/

	system("poweroff");
}
