cmake_minimum_required(VERSION 3.21)
project(CaGe)
include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH 
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake
)

include(cageToolchainOptions)

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

FetchContent_Declare(
        llvm-metavirt
        GIT_REPOSITORY ${GIT_REPO}
        GIT_TAG 24dd8bd4dde714229112badfa900a7348547a869
)
FetchContent_MakeAvailable(llvm-metavirt)

FetchContent_Declare(
        metacg
        GIT_REPOSITORY https://github.com/tudasc/MetaCG.git
        GIT_TAG a8118c718d3ba67c4f6110095046f56b8cea31f3
        OVERRIDE_FIND_PACKAGE
)
FetchContent_MakeAvailable(metacg)

cage_add_format_target(format-cage-sources
  "Formats project source files"
  TARGETS lib/*.cxx
          include/*.hxx
)

include_directories(include)
add_subdirectory(lib)
add_subdirectory(scripts)
if(PROJECT_IS_TOP_LEVEL)
  add_subdirectory(test)
endif()