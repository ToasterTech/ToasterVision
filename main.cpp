#include <iostream>
#include <math.h>
#include <time.h>

#include "opencv2/imgproc.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/opencv.hpp"
//#include "opencv2/gpu/gpu.hpp"
#include "ntcore/ntcore_cpp.h"
#include "ntcore/networktables/NetworkTable.h"
#include "ntcore/networktables/NetworkTableEntry.h"
#include "ntcore/networktables/NetworkTableInstance.h"
#include "cscore/cscore.h"
#include "cscore/cscore_oo.h"




//GLOBAL VARIABLES
//Debug Variables
bool DEBUG = false;           //Do we want to show the output screams?
bool NETWORK_TABLES = true; //Do we want to send values over Network Tables?
bool STREAM_OUTPUT  = true;
bool MEASURE_RUNTIME = true;
bool visionActive = false;
bool focusSet = false;
//bool previouslyNight = false;
//bool previouslyDay = false;

//Global Contour Variables
int cx1 = 0, cx2 = 0, cx = 0; //Center X points of Contour 1, Contour 2, and Center Point
int cy1 = 0, cy2 = 0, cy = 0; //The same thing but for y-coordinates
int area1, area2; //Two Largest Areas
std::vector<cv::Point> fst_contour, snd_contour; //Two Largest Contours
double angleToTarget;

//Camera Properties
double    FOV         = 78;
int    imageWidth  = 640;
int    imageHeight = 480;
double    focal_length_pixels = .5 * imageWidth / tan(FOV / 2);
double approximateDegreesPerPixel = FOV / imageWidth;

int    imageCenterX  = imageWidth / 2;
int    imageCenterY = imageHeight / 2;

//Global Mat Objects
cv::Mat currentFrame; //Output of Camera
cv::Mat HSVFrame;     //HSV Conversion Output
cv::Mat ThresholdFrame; //Output of HSV Filtering
cv::Mat contourFrame;   //Draws All Contours
cv::Mat outputFrame;    //Draws Top Two Contours with Target Point

//Global NetworkTables objects
nt::NetworkTableInstance inst;
std::shared_ptr<NetworkTable> table;

//Global CScore Objects
cs::CvSource    cvSource{"cvSource", cs::VideoMode::kMJPEG, imageWidth, imageHeight, 15};
cs::MjpegServer outputStreamServer{"outputStreamServer", 5800};


struct Right_Left_contour_sorter{

    bool operator()(const cv::RotatedRect& a, const cv::RotatedRect& b){
        return a.center.x < b.center.x;
    }

};

int main(){
	clock_t start, end;
	std::cout << "Hello World!\n"; //Quick Test Message
	
	if(NETWORK_TABLES){ //Set up Network Tables stuff
		inst = nt::NetworkTableInstance::GetDefault();
		inst.StartClientTeam(5332);
		table = inst.GetTable("vision_table");
	
		table->PutBoolean("JetsonOnline", true);
	}

	
	//Camera Setup
	
	cv::VideoCapture cap(0);

	if(!cap.isOpened()){
		return -1; //Make sure we can open the stream. If not, there is a problem.
	}
	//Uncomment the Line below if using a static image.
	//currentFrame = cv::imread("../PracticeImages/Image1.png", -1);
	
	contourFrame = cv::Mat::zeros(currentFrame.size(), CV_8UC3);

	       //system("v4l2-ctl -d /dev/video0 -c exposure_auto=1");
               //system("v4l2-ctl -d /dev/video0 -c exposure_absolute=5");



	if(STREAM_OUTPUT){
		outputStreamServer.SetSource(cvSource);
	}

	for(;;){ //Infinite Processing Loop
		//std::cout << "width: " << currentFrame.cols << "\n";
		//std::cout << "height: " << currentFrame.rows << "\n";

		if(MEASURE_RUNTIME){ start = clock(); }
		//if(VIDEO_STREAM) {cap >> currentFrame;} //get a new frame
		

		if(NETWORK_TABLES && ((table->GetString("visionMode", "day")=="day") && table->GetBoolean("changingMode", false))){
			system("v4l2-ctl -d /dev/video0 -c exposure_auto=1");
			system("v4l2-ctl -d /dev/video0 -c exposure_absolute=40");
			system("v4l2-ctl -d /dev/video0 -c gain=125");
			table->PutBoolean("changingMode", false);

			imageHeight = 480;
			imageWidth  = 640;

			visionActive = false;
		} else if(NETWORK_TABLES && ((table->GetString("visionMode", "day")=="night") && table->GetBoolean("changingMode", false))){
			system("v4l2-ctl -d /dev/video0 -c exposure_auto=1");
			system("v4l2-ctl -d /dev/video0 -c exposure_absolute=15");
			system("v4l2-ctl -d /dev/video0 -c gain=125");
			table->PutBoolean("changingMode", false);

            imageHeight = 240;
            imageWidth  = 360;

			visionActive = true;
		}


        cap >> currentFrame;
        cv::resize(currentFrame, currentFrame, cv::Size(imageWidth, imageHeight), 0, 0, cv::INTER_CUBIC);

		if(visionActive){
			//std::cout << "Vision Active: " << visionActive;
	
			cv::cvtColor(currentFrame, HSVFrame, cv::COLOR_BGR2HSV);

			//HSV Thresholding
			cv::Scalar lower_bound = cv::Scalar(28, 20, 160); //28, 28, 174
			cv::Scalar upper_bound = cv::Scalar(105, 205, 255);
		
			cv::inRange(HSVFrame, lower_bound, upper_bound, ThresholdFrame);

			std::vector<std::vector<cv::Point>> contours;
			std::vector<cv::RotatedRect> rectangleContours;

            cv::RotatedRect leftRectangle, rightRectangle;

			cv::findContours(ThresholdFrame, contours, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);

			//Find Contours with Four Sides
			for(std::vector<cv::Point> contour : contours){
				approxPolyDP(contour, contour, 0.03*cv::arcLength(contour, true), true);
									
				if(contour.size() == 4){
				    cv::RotatedRect rectangleContour = cv::minAreaRect(contour);
					rectangleContours.push_back(rectangleContour);
				}

				//std::cout << "Sides: " << contour.size() << "\n";
			}

			//Sort the Contours Left to right
			std::sort(rectangleContours.begin(), rectangleContours.end(), Right_Left_contour_sorter());

            cv::RotatedRect largestRectangle;
            cv::RotatedRect companionRectangle;

            //std::cout << "rectangle Contours Size: " << rectangleContours.size();
			if(rectangleContours.size() >  1){
                int largestArea = 0;
                int largestRectangleIndex = 0;

				for(int i = 0; i < rectangleContours.size(); i++){
                    //std::cout << "Rectangle X: " << rectangleContours[i].center.x << "\n";
                    //std::cout << "Rectangle Angle: " << rectangleContours[i].angle << "\n";

					float newArea = rectangleContours[i].size.area();

					if(newArea > largestArea){
						largestRectangle = rectangleContours[i];
						largestRectangleIndex = i;
						largestArea = newArea;
					}

				}


				if(largestRectangle.angle > -30) { //We are dealing with the one on the left

				    for(int i = largestRectangleIndex + 1; i < rectangleContours.size(); i++){

				        cv::RotatedRect currentRectangle = rectangleContours[i];

				        if(currentRectangle.angle > -80 && currentRectangle.angle < -30){
				            companionRectangle = currentRectangle;


                            break;
				        }

				    }


                } else {
                    //std::cout << "Largest Rect Index: " << largestRectangleIndex << "\n";
                    for(int i = largestRectangleIndex-1; i >= 0; i--){
                        //std::cout << "I: " << i << "\n";
                        cv::RotatedRect currentRectangle = rectangleContours[i];

                        //std::cout << "Current Rectangle Angle: " << currentRectangle.angle;

                        if(currentRectangle.angle > -30){
                            companionRectangle = currentRectangle;



                            break;
                        }

                    }


				}
                std::cout << "\n";



			} else { //There aren't enough contours
				//std::cout << "Not enough Rectangles" << "\n";
			}



            cx1 = largestRectangle.center.x;
            cy1 = largestRectangle.center.y;


            cx2 = companionRectangle.center.x;
            cy2 = companionRectangle.center.y;

            cx = (cx1 + cx2)/2;
            cy = (cy1 + cy2)/2;


			if(DEBUG){
				contourFrame = cv::Mat::zeros(currentFrame.size(), CV_8UC3);

				outputFrame = currentFrame.clone();

                cv::drawContours(contourFrame, contours, -1, cv::Scalar(255, 191, 0), 2);
                std::cout << "Rectangle Contours: " << rectangleContours.size() << "\n";

				cv::circle(outputFrame, cv::Point(cx1, cy1), 10, cv::Scalar(0, 0, 255), 10);
				cv::circle(outputFrame, cv::Point(cx2, cy2), 10, cv::Scalar(0, 0, 255), 10);
				cv::circle(outputFrame, cv::Point(cx, cy), 10, cv::Scalar(0, 0, 255), 10);

				imshow("Camera", currentFrame);
				imshow("HSV_Output", HSVFrame);
				imshow("Threshold Output", ThresholdFrame);
				imshow("Contour Output", contourFrame);
				imshow("Final Output", outputFrame);
			}
			//std::cout << "running\n";
		}

		if(STREAM_OUTPUT){
			outputFrame = currentFrame.clone();
			cv::circle(outputFrame, cv::Point(cx, cy), 10, cv::Scalar(0, 0, 255), 10);
			cvSource.PutFrame(outputFrame);
		}

			

		if(cv::waitKey(30) >= 0 || (NETWORK_TABLES && table->GetBoolean("Shutdown", false))){
			break;
		}

		if(MEASURE_RUNTIME && NETWORK_TABLES){
			end = clock();

			table->PutNumber("Runtime", (double) (end-start)/CLOCKS_PER_SEC);
		}
	}
	/**
	for(;;){
		testEntry.setDouble(5332.0);
	}**/

	//system("sudo poweroff");
}
