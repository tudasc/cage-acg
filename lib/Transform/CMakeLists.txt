target_sources(plugin PUBLIC CallGraphGenerator.cpp)
target_link_libraries(plugin ${libmetacg} ${libspdlog})
