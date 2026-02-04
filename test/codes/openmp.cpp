// RUN: env GENCC_CG_NAME=%t.mcg %cage_cxx -fopenmp %s -o %t && %filecheck %s --input-file=%t.mcg
// CHECK: "nodes":

// REQUIRES: openmp

#include <cstdio>

int main(int argc, char** argv) {
#pragma omp parallel for
  for (int i = 0; i < argc; i++) {
    printf("%s", argv[i]);
  }
}