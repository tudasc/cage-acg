#include <dlfcn.h>
#include <cstdio>
#include "lib.h"

int main(int argc, char** argv){
    printf("Getting Handle\n");
    /*void *handle = dlopen("libmyFunction.so",1);
    if (!handle){
      printf(" error: cannot locate the library!\n");
      return 1;
    }
    printf("Getting Function\n");

    int (*func)() = (int (*)())dlsym(handle, "myFunction");
    if (!func){
      printf("error: cannot locate the symbol!\n");
      return 1;
    }*/
    printf("Calling Function\n");

    printf("The function returns: %d\n", myFunction());
    return 0;
}


