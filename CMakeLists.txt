cmake_minimum_required(VERSION 3.5)
project(ToasterVision)

find_Package(wpilib REQUIRED)

add_executable(ToasterVision main.cpp)
target_link_libraries(ToasterVision ntcore)
