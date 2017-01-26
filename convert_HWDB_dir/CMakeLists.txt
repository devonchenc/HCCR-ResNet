cmake_minimum_required(VERSION 2.8)

project(convert_HWDB)

add_subdirectory(caffe/util)
add_subdirectory(caffe/proto)

add_executable(${PROJECT_NAME} "convert_HWDB.cpp")
target_link_libraries(convert_HWDB caffe_util)
target_link_libraries(convert_HWDB caffe_proto)
target_link_libraries(convert_HWDB glog)
target_link_libraries(convert_HWDB protobuf)
target_link_libraries(convert_HWDB lmdb)
