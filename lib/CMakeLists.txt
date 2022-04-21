add_library(plugin MODULE Plugin.cpp)

get_target_property(pluginName plugin NAME)
set_target_properties(plugin PROPERTIES PROBABLE_PATH ${CMAKE_CURRENT_BINARY_DIR}/lib${pluginName}.so)


add_subdirectory(Analysis)
add_subdirectory(Runtime)
add_subdirectory(Transform)