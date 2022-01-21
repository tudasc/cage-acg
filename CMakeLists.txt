cmake_minimum_required(VERSION 3.17)
project(CallGraphGeneration)
set(CMAKE_CXX_STANDARD 14)

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM/Clang version " ${LLVM_VERSION})

include_directories(${LLVM_INCLUDE_DIRS})

add_library(genCC MODULE genCC.cpp)