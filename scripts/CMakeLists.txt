include(GNUInstallDirs)

cage_find_llvm_progs(CAGE_CLANG_EXEC "clang-${LLVM_VERSION_MAJOR};clang")
cage_find_llvm_progs(CAGE_CLANGCXX_EXEC "clang++-${LLVM_VERSION_MAJOR};clang++")

find_package(MPI QUIET)

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

function(cage_find_mpi_vendor_helper symbol ret_value)
  if(NOT MPI_FOUND)
    set(${ret_value} false)
    return()
  endif()

  set(MPI_TEST_CODE
   "#include <mpi.h>
    int main(void) {
      #if !defined(${symbol})
        an_illegal_stmt
      #endif
      return 0;
    }")

  set(CMAKE_REQUIRED_INCLUDES ${MPI_C_INCLUDE_DIRS})
  set(CMAKE_REQUIRED_FLAGS "${MPI_C_COMPILE_FLAGS}")
  set(CMAKE_REQUIRED_DEFINITIONS "")
  set(CMAKE_REQUIRED_LIBRARIES "${MPI_C_LIBRARIES}")

  check_c_source_compiles("${MPI_TEST_CODE}" ${ret_value})
endfunction()

function(set_cage_mpi_compiler)
  # Note on mpich & Intel: Intel may also define \"MPICH_NAME\"
  # in mpi.h, so check MPICH last
  list(APPEND l_vendor OPEN_MPI INTEL_MPI MPICH)
  list(APPEND l_symbol OPEN_MPI I_MPI_VERSION MPICH_NAME)
  list(APPEND l_env_c OMPI_CC I_MPI_CC MPICH_CC)
  list(APPEND l_env_cxx OMPI_CXX I_MPI_CXX MPICH_CXX)

  foreach(vendor symbol env_c env_cxx IN ZIP_LISTS l_vendor l_symbol l_env_c l_env_cxx)
    cage_find_mpi_vendor_helper(${symbol} CAGE_HAVE_${vendor})
    if(CAGE_HAVE_${vendor})
      if(MPI_C_FOUND)
        set(CAGE_MPICC
            "env ${env_c}=${CAGE_CLANG_EXEC} ${MPI_C_COMPILER}"
            CACHE STRING "TypeART MPICC compiler command for scripts"
          )
        mark_as_advanced(CAGE_MPICC)
      endif()
      if(MPI_CXX_FOUND)
        set(CAGE_MPICXX
            "env ${env_cxx}=${CAGE_CLANGCXX_EXEC} ${MPI_CXX_COMPILER}"
            CACHE STRING "TypeART MPICXX compiler command for scripts"
        )
        mark_as_advanced(CAGE_MPICXX)
      endif()
      break()
    endif()
  endforeach()
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

if(MPI_FOUND)
  set_cage_mpi_compiler()
endif()

if(MPI_C_FOUND)
  cage_configure_wrapper(
    cage-wrapper.in cage-mpicc
    "${CAGE_MPICC}"
    1
  )
endif()

if(MPI_CXX_FOUND)
  cage_configure_wrapper(
    cage-wrapper.in cage-mpic++
    "${CAGE_MPICXX}"
    1
  )
endif()

install(
  PROGRAMS
    ${CMAKE_CURRENT_BINARY_DIR}/cage-clang
    ${CMAKE_CURRENT_BINARY_DIR}/cage-clang++
    $<$<BOOL:${MPI_C_FOUND}>:${CMAKE_CURRENT_BINARY_DIR}/cage-mpicc>
    $<$<BOOL:${MPI_CXX_FOUND}>:${CMAKE_CURRENT_BINARY_DIR}/cage-mpic++>
  DESTINATION ${CMAKE_INSTALL_BINDIR}
)
