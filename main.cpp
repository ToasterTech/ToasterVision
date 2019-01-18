#include <iostream>

#include "opencv2/imgproc.hpp"
#include "opencv2/highgui.hpp"
#include "ntcore/ntcore_cpp.h"
#include "ntcore/networktables/NetworkTable.h"
#include "ntcore/networktables/NetworkTableEntry.h"
#include "ntcore/networktables/NetworkTableInstance.h"

int main(){	
	std::cout << "Hello World!\n";

	auto inst = nt::NetworkTableInstance::GetDefault();
	auto table = inst.GetTable("vision_table");

	nt::NetworkTableEntry testEntry = table->GetEntry("TestEntry");

	/**
	for(;;){
		testEntry.setDouble(5332.0);
	}**/
	

}
