include(GNUInstallDirs)

cage_find_llvm_progs(CAGE_CLANG_EXEC "clang-${LLVM_VERSION_MAJOR};clang")
cage_find_llvm_progs(CAGE_CLANGCXX_EXEC "clang++-${LLVM_VERSION_MAJOR};clang++")

function(cage_configure_wrapper input output compiler relocatable)
  set(CAGE_COMPILER ${compiler})
  set(CAGE_RELOCATABLE ${relocatable})

  string(TOLOWER ${PROJECT_NAME} CAGE_DIR_NAME)

  if(CAGE_RELOCATABLE EQUAL 1)
    set(CAGE_INCLUDE_DIRS
        "-I${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR}/${CAGE_DIR_NAME}"
    )
    set(CAGE_LIB_DIR ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR})
    set(CAGE_BINARY_DIR ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR})
  else()
    set(CAGE_INCLUDE_DIRS "-I${PROJECT_SOURCE_DIR}/lib/Transform -I${PROJECT_SOURCE_DIR}/include")
    if(LIBRARY_OUTPUT_PATH)
      set(CAGE_LIB_DIR ${LIBRARY_OUTPUT_PATH})
    else()
      set(CAGE_LIB_DIR ${CMAKE_CURRENT_BINARY_DIR}/../lib)
    endif()

    if(EXECUTABLE_OUTPUT_PATH)
        set(CAGE_BINARY_DIR ${EXECUTABLE_OUTPUT_PATH})
    else()
        set(CAGE_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})
    endif()
  endif()
  
  cage_target_generate_file(${input} ${output})
endfunction()

# Build-tree versions (for testing)
cage_configure_wrapper(
  cage-wrapper.in 
  cage-clang-test 
  ${CAGE_CLANG_EXEC} 
  0
)
cage_configure_wrapper(
  cage-wrapper.in 
  cage-clang++-test 
  ${CAGE_CLANGCXX_EXEC} 
  0
)

# Install-tree versions
cage_configure_wrapper(
  cage-wrapper.in 
  cage-clang 
  ${CAGE_CLANG_EXEC} 
  1
)
cage_configure_wrapper(
  cage-wrapper.in 
  cage-clang++ 
  ${CAGE_CLANGCXX_EXEC} 
  1
)

install(
  PROGRAMS
    ${CMAKE_CURRENT_BINARY_DIR}/cage-clang
    ${CMAKE_CURRENT_BINARY_DIR}/cage-clang++
  DESTINATION ${CMAKE_INSTALL_BINDIR}
)
