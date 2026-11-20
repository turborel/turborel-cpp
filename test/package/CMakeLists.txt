# A program that finds the installed package and links its target, as a
# user of turborel-cpp does (RULES.md D9). Run by the test package.consume.
cmake_minimum_required(VERSION 3.25)
project(turborel-cpp-consumer LANGUAGES CXX)

find_package(turborel-cpp 0.0.0 EXACT CONFIG REQUIRED)

add_executable(consumer consumer.cpp)
target_link_libraries(consumer PRIVATE turborel::turborel-cpp)
