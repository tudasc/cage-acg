#include "library.h"

#include <iostream>

extern "C"
void getGCC(void* data) {
    printf("GotCalled\n");
    printf("%s\n",data);
}
