#include <dlfcn.h>
#include <cstdio>

int a=5;

int main(int argc, char** argv){
    printf("Getting Handle\n");
    void *handle = dlopen("slib.so",1);
    if (!handle){
      printf(" error: cannot locate the library!\n");
      return 1;
    }
    printf("Getting Function\n");

    int (*func)() = (int (*)())dlsym(handle, "myFunction");
    if (!func){
      printf("error: cannot locate the symbol!\n");
      return 1;
    }
    printf("Calling Function\n");

    printf("The function returns: %d\n", func());
    return 0;
}

