add_library(cage MODULE Plugin.cpp)

target_link_libraries(cage metacg::metacg)

target_compile_options(cage PUBLIC "-fno-rtti")
get_target_property(pluginName cage NAME)
set_target_properties(cage PROPERTIES PROBABLE_PATH ${CMAKE_CURRENT_BINARY_DIR}/lib${pluginName}.so)

add_subdirectory(Analysis)
add_subdirectory(Runtime)
add_subdirectory(Transform)