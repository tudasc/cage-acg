cmake_minimum_required(VERSION 3.17)
project(CallGraphGeneration)
set(CMAKE_CXX_STANDARD 17)

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM/Clang version " ${LLVM_VERSION})


include_directories(${LLVM_INCLUDE_DIRS})


find_package(metacg REQUIRED)
find_package(spdlog REQUIRED)

include_directories(include)
add_subdirectory(lib)

#configure tests last
add_subdirectory(test)

get_property(GENCC_PLUGIN_DIR TARGET cage PROPERTY PROBABLE_PATH)
get_property(GENCC_RUNTIME_DIR TARGET genCCRT PROPERTY PROBABLE_PATH)
get_property(libgenCC TARGET cage PROPERTY PROBABLE_PATH)
get_property(runtimeComponent TARGET genCCRT PROPERTY PROBABLE_PATH)


configure_file( ${CMAKE_SOURCE_DIR}/util/c_pp_wrapper_no_opt.sh  ${CMAKE_SOURCE_DIR}/util/c_pp_wrapper_no_opt_config.sh @ONLY)
configure_file( ${CMAKE_SOURCE_DIR}/util/c_pp_wrapper_all_opt.sh  ${CMAKE_SOURCE_DIR}/util/c_pp_wrapper_all_opt_config.sh @ONLY)
configure_file( ${CMAKE_SOURCE_DIR}/util/c_pp_wrapper_only_gencc.sh  ${CMAKE_SOURCE_DIR}/util/c_pp_wrapper_only_gencc_config.sh @ONLY)
configure_file( ${CMAKE_SOURCE_DIR}/util/c_wrapper_no_opt.sh  ${CMAKE_SOURCE_DIR}/util/c_wrapper_no_opt_config.sh @ONLY)
configure_file( ${CMAKE_SOURCE_DIR}/util/c_wrapper_all_opt.sh  ${CMAKE_SOURCE_DIR}/util/c_wrapper_all_opt_config.sh @ONLY)
configure_file( ${CMAKE_SOURCE_DIR}/util/c_wrapper_only_gencc.sh  ${CMAKE_SOURCE_DIR}/util/c_wrapper_only_gencc_config.sh @ONLY)
