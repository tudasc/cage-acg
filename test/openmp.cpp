#include <cstdio>

int main(int argc, char** argv) {
#pragma omp parallel for
  for (int i = 0; i < argc; i++) {
    printf("%s", argv[i]);
  }
}