cmake_minimum_required(VERSION 3.17)
project(CallGraphGeneration)
set(CMAKE_CXX_STANDARD 17)

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM/Clang version " ${LLVM_VERSION})

include_directories(${LLVM_INCLUDE_DIRS})

if(NOT DEFINED metaCGDir)
  message(WARNING "MetaCG directory was not found"
          "Use -DmetaCGDir=<PATH> to set path")
  #find_package(MetaCG REQUIRED CONFIG)
endif ()

set(libmetacg ${metaCGDir}/build/graph/libmetacg.so)
set(libspdlog ${metaCGDir}/build/spdlog-build/libspdlogd.so)
include_directories(${metaCGDir}/extern/json/include/)
include_directories(${metaCGDir}/pgis/lib/include/)
include_directories(${metaCGDir}/extern/src/extrap/extrap-3.0/include/)
include_directories(${metaCGDir}/build/)

#Builds and compiles without this, but who knows if it breaks anything if we remove this
#include_directories(${metaCGDir}/spdlog-src/include/)

include_directories(${metaCGDir}/graph/include/)

include_directories(include)
add_subdirectory(lib)