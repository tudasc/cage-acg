set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules)

include(llvm-util)
include(clang-format)
include(cage-target-util)

if(PROJECT_IS_TOP_LEVEL)
  if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    # set default install path
    set(CMAKE_INSTALL_PREFIX
        "${CaGe_SOURCE_DIR}/install/cage"
        CACHE PATH "Default install path" FORCE
    )
    message(STATUS "Installing to (default): ${CMAKE_INSTALL_PREFIX}")
  endif()
endif()

if(NOT DEFINED CAGE_LOG_LEVEL)
  if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CAGE_LOG_LEVEL 0 CACHE STRING "Logging level (0-3)")
  else()
    set(CAGE_LOG_LEVEL 2 CACHE STRING "Logging level (0-3)")
  endif()
endif()
message(STATUS "CaGe log level ${CAGE_LOG_LEVEL}")