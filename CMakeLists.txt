cmake_minimum_required(VERSION 3.17)
project(CallGraphGeneration)
set(CMAKE_CXX_STANDARD 17)

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM/Clang version " ${LLVM_VERSION})

include_directories(${LLVM_INCLUDE_DIRS})

#
set(metaCGDir /media/tim/Volume/Studium/Master/Semester3/MasterArbeit/MetaCG)
set(libmetacg ${metaCGDir}/build/graph/libmetacg.so)
set(libspdlog ${metaCGDir}/build/spdlog-build/libspdlogd.so)


include_directories(${metaCGDir}/extern/json/include/)
include_directories(${metaCGDir}/pgis/lib/include/)
include_directories(${metaCGDir}/extern/src/extrap/extrap-3.0/include/)
include_directories(${metaCGDir}/spdlog-src/include/)


include_directories(${metaCGDir}/graph/include/)

add_library(genCC MODULE genCC.cpp LLVMTypeHierarchy.h)
target_compile_options(genCC PUBLIC "-fno-rtti")
target_link_libraries(genCC PRIVATE ${libmetacg} ${libspdlog})
