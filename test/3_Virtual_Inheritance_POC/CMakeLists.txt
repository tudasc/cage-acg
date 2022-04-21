get_property(libgenCC_path TARGET plugin PROPERTY PROBABLE_PATH)
get_property(runtimeComponent TARGET genCCRT PROPERTY PROBABLE_PATH)

FILE(GLOB SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/  *.cpp)
string(JOIN " " SOURCES ${SOURCES})

configure_file( ${CMAKE_SOURCE_DIR}/util/ownCompilePipelineTemplate.sh ${CMAKE_CURRENT_SOURCE_DIR}/compile.sh @ONLY)