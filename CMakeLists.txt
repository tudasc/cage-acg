cmake_minimum_required(VERSION 3.17)
project(CaGe)
set(CMAKE_CXX_STANDARD 17)

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM/Clang version " ${LLVM_VERSION})

if("${LLVM_VERSION_MAJOR}" VERSION_LESS 18)
    message(FATAL_ERROR "Found LLVM ${LLVM_VERSION_MAJOR}, but need LLVM 18 or above")
endif()

include_directories(${LLVM_INCLUDE_DIRS})


find_package(metacg REQUIRED)
find_package(spdlog REQUIRED)

include_directories(include)
add_subdirectory(lib)

#configure tests last
add_subdirectory(test)