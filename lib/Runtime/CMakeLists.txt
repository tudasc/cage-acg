add_library(genCCRT SHARED library.cpp)
target_link_libraries(genCCRT PUBLIC ${libmetacg} ${libspdlog})
get_target_property(runtimeName genCCRT NAME)
set_target_properties(genCCRT PROPERTIES PROBABLE_PATH ${CMAKE_CURRENT_BINARY_DIR}/lib${runtimeName}.so)