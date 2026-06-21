cmake_minimum_required(VERSION 3.15)
project(test_package CXX)

find_package(qbuem_json CONFIG REQUIRED)

add_executable(example example.cpp)
target_link_libraries(example PRIVATE qbuem_json::qbuem_json)
target_compile_features(example PRIVATE cxx_std_20)
