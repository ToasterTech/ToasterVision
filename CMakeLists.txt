cmake_minimum_required(VERSION 3.5)
project(ToasterVision)

find_Package(wpilib REQUIRED)
find_Package(OpenCV REQUIRED)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

include_directories(${OpenCV_INCLUDE_DIRS})
include_directories(/usr/local/include)
include_directories(/usr/local/lib)

add_executable(ToasterVision main.cpp)

target_link_libraries(ToasterVision ntcore)
target_link_libraries(ToasterVision cscore)
target_link_libraries(ToasterVision ${OpenCV_LIBS})
