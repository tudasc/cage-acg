add_library(cage SHARED plugin.cxx)

target_link_libraries(cage PRIVATE metavirt_Types)
target_link_libraries(cage PRIVATE metacg::metacg)

add_library(cage::cage ALIAS cage)

# RPATH settings
# Ensure that the install RPATH is set to $ORIGIN (on Linux)
set_target_properties(cage PROPERTIES INSTALL_RPATH "$ORIGIN")

cage_target_define_file_basename(cage)
target_compile_definitions(cage PRIVATE CAGE_LOG_LEVEL=${CAGE_LOG_LEVEL})

install(
  TARGETS cage
  EXPORT CaGeTargets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(
  FILES ${PROJECT_SOURCE_DIR}/include/cage/generator.hxx
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/cage
)

install(
  EXPORT CaGeTargets
  FILE CaGeTargets.cmake
  NAMESPACE cage::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CaGe
)

#necessary due to semi broken metacg package version
add_json(cage)
add_spdlog_libraries(cage)

# TODO: observe this fix for export errors: dependencies added by metacg functions are PUBLIC
# but not needed in the interface of the cage plugin.
set_target_properties(cage PROPERTIES INTERFACE_LINK_LIBRARIES "")

add_subdirectory(Transform)