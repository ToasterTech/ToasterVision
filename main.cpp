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
bool DEBUG = true;           //Do we want to show the output screams?
bool NETWORK_TABLES = false; //Do we want to send values over Network Tables?
bool STREAM_OUTPUT  = true;
bool MEASURE_RUNTIME = true;
bool visionActive = true;
bool focusSet = false;

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
	clock_t start, end;
	std::cout << "Hello World!\n"; //Quick Test Message
	
	if(NETWORK_TABLES){ //Set up Network Tables stuff
		inst = nt::NetworkTableInstance::GetDefault();
		inst.StartClientTeam(5332);
		table = inst.GetTable("vision_table");
	
		table->PutBoolean("JetsonOnline", true);
	}
	
	cv::VideoCapture cap(1);

	contourFrame = cv::Mat::zeros(currentFrame.size(), CV_8UC3);

	if(STREAM_OUTPUT){
		outputStreamServer.SetSource(cvSource);
	}

	for(;;){ //Infinite Processing Loop

		if(MEASURE_RUNTIME){ start = clock(); }
		
        //This is for the Day-to-night switching. Smaller Camera Frames = Faster Processing
		if(NETWORK_TABLES && ((table->GetString("visionMode", "day")=="day") && table->GetBoolean("changingMode", false))){
		    system("v4l2-ctl -d /dev/video0 -c exposure_auto=1");
			system("v4l2-ctl -d /dev/video0 -c exposure_absolute=40");
			//system("v4l2-ctl -d /dev/video0 -c gain=125");
			table->PutBoolean("changingMode", false);

			imageHeight = 480;
			imageWidth  = 640;

			visionActive = false;
		} else if(NETWORK_TABLES && ((table->GetString("visionMode", "day")=="night") && table->GetBoolean("changingMode", false))){
			system("v4l2-ctl -d /dev/video0 -c exposure_auto=1");
			system("v4l2-ctl -d /dev/video0 -c exposure_absolute=5");
			//system("v4l2-ctl -d /dev/video0 -c gain=125");
			table->PutBoolean("changingMode", false);

            imageHeight = 240;
            imageWidth  = 320;

			visionActive = true;
		}


        cap >> currentFrame;
        cv::resize(currentFrame, currentFrame, cv::Size(imageWidth, imageHeight), 0, 0, cv::INTER_CUBIC);

        int numContoursOfArea = 0;
        //std::cout <<  "Height: " << currentFrame.rows;
		if(visionActive){


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
                    std::cout << "Angle: " << rectangleContours[i].angle << ", ";

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
			//std::cout << "running\n";
		}

		if(STREAM_OUTPUT){
			outputFrame = currentFrame.clone();
			cv::circle(outputFrame, cv::Point(cx, cy), 10, cv::Scalar(0, 0, 255), 10);
			cvSource.PutFrame(outputFrame);
		}

			

		if(cv::waitKey(30) >= 0){
			break;
		} else if((NETWORK_TABLES && table->GetBoolean("Shutdown", false))){
            system("sudo poweroff");
            break;
        }

		if(MEASURE_RUNTIME && NETWORK_TABLES){
			end = clock();

			table->PutNumber("Runtime", (double) (end-start)/CLOCKS_PER_SEC);
		}
	}
}
