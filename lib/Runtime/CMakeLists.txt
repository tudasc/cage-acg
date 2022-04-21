set(CMAKE_CXX_STANDARD 14)
add_library(genCCRT SHARED library.cpp)
get_target_property(runtimeName genCCRT NAME)
set_target_properties(genCCRT PROPERTIES PROBABLE_PATH ${CMAKE_CURRENT_BINARY_DIR}/lib${runtimeName}.so)