cmake_minimum_required(VERSION 2.8)
find_package(Qt4 REQUIRED)
include(${QT_USE_FILE})
include_directories(${CMAKE_CURRENT_LIST_DIR} ${QT_INCLUDES})
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
add_executable(img-sub main.cpp)
target_link_libraries(img-sub ${QT_LIBRARIES})
