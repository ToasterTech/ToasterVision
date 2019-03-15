#include <iostream>
#include <math.h>
#include <time.h>
#include <unistd.h>

#include "opencv2/imgproc.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/opencv.hpp"
//#include "opencv2/gpu/gpu.hpp"
#include "ntcore_cpp.h"
#include "networktables/NetworkTable.h"
#include "networktables/NetworkTableEntry.h"
#include "networktables/NetworkTableInstance.h"
#include "cscore.h"
#include "cscore_oo.h"
#include "wpi/raw_ostream.h"


//GLOBAL VARIABLES
//Debug Variables
bool DEBUG = false;           //Do we want to show the output screams?
bool NETWORK_TABLES = true; //Do we want to send values over Network Tables?
bool STREAM_OUTPUT  = true;
bool MEASURE_RUNTIME = true;

//Global Contour Variables
int cx1 = 0, cx2 = 0, cx = 0; //Center X points of Contour 1, Contour 2, and Center Point
int cy1 = 0, cy2 = 0, cy = 0; //The same thing but for y-coordinates
double angleToTarget;

//Camera Properties
double    FOV      = 60;
int    imageWidth  = 320;
int    imageHeight = 240;
double    focal_length_pixels = .5 * imageWidth / tan(FOV / 2);
double approximateDegreesPerPixel = FOV / imageWidth;

int    imageCenterX  = imageWidth / 2;

int totalTime = 0;
int totalCycles = 0;


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


//This exists so we can sort the contours left-to-right
struct Right_Left_contour_sorter{

    bool operator()(const cv::RotatedRect& a, const cv::RotatedRect& b){
        return a.center.x < b.center.x;
    }

};


int main(){
    //cv::VideoCapture cap(0);
	clock_t start, end;
	//wpi::outs() << "Hello World\n";
	std::cout << "Hello World!" << std::endl; //Quick Test Message

	contourFrame = cv::Mat::zeros(cv::Size(640, 480), CV_8UC3);
	//usleep(2 * 10^7);
	if(NETWORK_TABLES){ //Set up Network Tables stuff
		inst = nt::NetworkTableInstance::GetDefault();
		inst.StartClientTeam(5332);
		table = inst.GetTable("vision_table");

		table->PutBoolean("JetsonOnline", true);

		std::cout  << "Waiting on Network Tables: \n" << std::endl;
        while(!inst.IsConnected()){

        }
	std::cout  << "CONNECTED TO NETWORK TABLES" << std::endl;
	}



	std::cout << "about to do stream output" << std::endl;
	if(STREAM_OUTPUT){
		outputStreamServer.SetSource(cvSource);
		cvSource.PutFrame(contourFrame);
	}
	std::cout << "Open Camera 0" << std::endl;

	//system("sleep 10");
	//table->PutBoolean("DoneSleeping", true);
	//return 0;
	
	//cv::VideoCapture cap("/dev/video0"); //This Line is causing a GStreamer Error
	
	if(!cap.isOpened()){
		std::cout << "Open Failed" << std::endl;
	}

	std::cout << "Entering Loop" << std::endl;
	for(;;){ //Infinite Processing Loop

        cap >> currentFrame;

        if(currentFrame.empty()){
            continue;
        }

        cv::resize(currentFrame, currentFrame, cv::Size(imageWidth, imageHeight), 0, 0, cv::INTER_CUBIC);

        int numContoursOfArea = 0;
        //std::cout <<  "Height: " << currentFrame.rows;


        //Convert to HSV
        cv::cvtColor(currentFrame, HSVFrame, cv::COLOR_BGR2HSV);


        //HSV Thresholding
        cv::Scalar lower_bound = cv::Scalar(10, 10, 130); //28, 28, 174
        cv::Scalar upper_bound = cv::Scalar(170, 255, 255); //105, 205, 255

        cv::inRange(HSVFrame, lower_bound, upper_bound, ThresholdFrame);
        std::vector<std::vector<cv::Point>> contours;
        std::vector<cv::RotatedRect> rectangleContours;

        cv::RotatedRect leftRectangle, rightRectangle;

        cv::findContours(ThresholdFrame, contours, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);

        //Find Contours with Four Sides
        for(std::vector<cv::Point> contour : contours){
            approxPolyDP(contour, contour, 0.03*cv::arcLength(contour, true), true); //The .03 may be adjusted

            if(contour.size() == 4){
                cv::RotatedRect rectangleContour = cv::minAreaRect(contour);
                rectangleContours.push_back(rectangleContour);
            }

            //std::cout << "Sides: " << contour.size() << "\n";
        }

        //Sort the remaining contours Left to right
        std::sort(rectangleContours.begin(), rectangleContours.end(), Right_Left_contour_sorter());

        cv::RotatedRect largestRectangle;
        cv::RotatedRect companionRectangle;

        if(rectangleContours.size() >  1){
            int largestArea = 0;
            int largestRectangleIndex = 0;

            for(int i = 0; i < rectangleContours.size(); i++){ ///Get Contours of Minimum Area;

                float newArea = rectangleContours[i].size.area();
                //std::cout << "area: " << newArea << ", ";
                //std::cout << "Angle: " << rectangleContours[i].angle << ", ";

                if(newArea < 100){ //This will filter out some noise
                    continue;
                } else if(newArea > largestArea){
                    largestRectangleIndex = i;
                    largestRectangle = rectangleContours[i];
                    largestArea = newArea;
                }

                numContoursOfArea = numContoursOfArea + 1;

            }


            std::sort(rectangleContours.begin(), rectangleContours.end(), Right_Left_contour_sorter());

            if(largestRectangle.angle > -30) { //We are dealing with the one on the left

                for(int i = largestRectangleIndex-1; i > -1; i--){

                    cv::RotatedRect currentRectangle = rectangleContours[i];

                    //So, because the contours are sorted left to right, its gonna find the first rectangle to the right.
                    if(currentRectangle.angle < -30){ //Find one for the right.
                        companionRectangle = currentRectangle;


                        break;
                    }

                }


            } else {
                for(int i = largestRectangleIndex + 1; i < rectangleContours.size(); i++){
                    cv::RotatedRect currentRectangle = rectangleContours[i];

                    if(currentRectangle.angle > -30){
                        companionRectangle = currentRectangle;



                        break;
                    }

                }


            }
            std::cout << "\n";



        } else {

        }

        cx1 = largestRectangle.center.x;
        cy1 = largestRectangle.center.y;


        cx2 = companionRectangle.center.x;
        cy2 = companionRectangle.center.y;

        cx = (cx1 + cx2)/2;
        cy = (cy1 + cy2)/2;

        angleToTarget = (imageCenterX - cx) * approximateDegreesPerPixel;

        if(NETWORK_TABLES){ //Publish the Values to Network Tables
            table->PutNumber("Angle", angleToTarget);
            table->PutNumber("centerX", cx);
            table->PutNumber("centerY", cy);
        }


        if(DEBUG){
            contourFrame = cv::Mat::zeros(currentFrame.size(), CV_8UC3);

            outputFrame = currentFrame.clone();

            cv::drawContours(contourFrame, contours, -1, cv::Scalar(255, 191, 0), 2);

            std::cout << "Rectangle Contours: " << rectangleContours.size() << "\n";
            std::cout << "Number of Contours Area; " << numContoursOfArea << "\n";

            cv::circle(outputFrame, cv::Point(cx1, cy1), 10, cv::Scalar(0, 0, 255), 10);
            cv::circle(outputFrame, cv::Point(cx2, cy2), 10, cv::Scalar(0, 0, 255), 10);
            cv::circle(outputFrame, cv::Point(cx, cy), 10, cv::Scalar(0, 0, 255), 10);

            imshow("Camera", currentFrame);
            imshow("HSV_Output", HSVFrame);
            imshow("Threshold Output", ThresholdFrame);
            imshow("Contour Output", contourFrame);
            imshow("Final Output", outputFrame);
        }

		if(STREAM_OUTPUT){
			outputFrame = currentFrame.clone();
			cv::circle(outputFrame, cv::Point(cx, cy), 10, cv::Scalar(0, 0, 255), 10);
			cvSource.PutFrame(outputFrame);
		}



        if(cv::waitKey(30) >= 0){
			break;
		} else if((NETWORK_TABLES && table->GetBoolean("Shutdown", false))){
            break;
        }

		if(MEASURE_RUNTIME && NETWORK_TABLES){
			end = clock();
            totalTime = totalTime + ((end-start)/CLOCKS_PER_SEC);
            totalCycles = totalCycles + 1;
			table->PutNumber("Runtime", (double) totalTime/totalCycles);
		}
	}
}
