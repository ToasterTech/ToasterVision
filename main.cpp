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
	if(!cap.isOpened()){
		return -1; //Make sure we can open the stream. If not, there is a problem.
	}

	for(;;){ //Infinite Processing Loop
		cv::Mat currentFrame;
		cap >> currentFrame; //get a new frame

		

		if(DEBUG){
			imshow("Camera", frame);
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
