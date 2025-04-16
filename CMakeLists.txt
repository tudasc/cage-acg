cmake_minimum_required(VERSION 3.17)
project(CaGe)
set(CMAKE_CXX_STANDARD 17)
include(FetchContent)
find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM/Clang version " ${LLVM_VERSION})

if("${LLVM_VERSION_MAJOR}" VERSION_LESS 18)
    message(STATUS "Found LLVM ${LLVM_VERSION_MAJOR}, but need LLVM 18 or above")
endif()

include_directories(${LLVM_INCLUDE_DIRS})

if(DEFINED USERNAME_PW)
    set(GIT_REPO https://${USERNAME_PW}@github.com/ahueck/llvm-metavirt.git)
else()
    set(GIT_REPO https://github.com/ahueck/llvm-metavirt.git)
endif()

message(STATUS "Loglevel ${VIRTCALL_LOG_LEVEL}")
add_compile_definitions(VIRTCALL_LOG_LEVEL)
FetchContent_Declare(
        llvm-metavirt
        GIT_REPOSITORY ${GIT_REPO}
        GIT_TAG devel
        GIT_SHALLOW 1
)

FetchContent_MakeAvailable(llvm-metavirt)



FetchContent_Declare(
        metacg
        GIT_REPOSITORY https://github.com/tudasc/MetaCG.git
        GIT_TAG devel
        GIT_SHALLOW 1
        OVERRIDE_FIND_PACKAGE
)
FetchContent_MakeAvailable(metacg)

include_directories(include)
add_subdirectory(lib)