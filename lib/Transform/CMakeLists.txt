target_sources(cage PUBLIC CallGraphGenerator.cpp)
target_link_libraries(cage ${libmetacg} ${libspdlog})
