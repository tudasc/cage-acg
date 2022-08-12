#include "lib.h"

int a=10;

extern "C" int myFunction() {
    return a;
}
