add_library(cage SHARED Plugin.cpp)

target_link_libraries(cage PRIVATE metavirt_Types)
target_link_libraries(cage PRIVATE metacg::metacg)
#necessary due to semi broken metacg package version
add_json(cage)
add_spdlog_libraries(cage)
add_subdirectory(Transform)