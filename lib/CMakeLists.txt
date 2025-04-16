add_library(cage SHARED Plugin.cpp)

target_link_libraries(cage PRIVATE metavirt_Types)
target_link_libraries(cage PRIVATE metacg::metacg)
add_subdirectory(Transform)