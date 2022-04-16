#include "Runtime/library.h"

#include <iostream>

void getGCC(void* data) {
    printf("GotCalled\n");
    printf("%s\n",data);
}
