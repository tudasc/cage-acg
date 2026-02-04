// RUN: env GENCC_CG_NAME=%t.mcg %cage_cxx %s -o %t && %filecheck %s --input-file=%t.mcg
// CHECK: "nodes":

#include <cstdio>

struct Base {
  virtual int bar() {
    return 1;
  }  // some value
};

struct Derived : public Base {
  virtual int bar() {
    return 2;
  }  // some value
};

int add(Base* obj, int v) {
  return obj->bar() + v;
}

int main(int argc, char** argv) {
  int ifSwitch;
  scanf("%d", &ifSwitch);

  Base* b = 0;

  if (ifSwitch < 5) {
    b = new Derived();
  } else {
    b = new Base();
  }

  printf("%d\n", add(b, ifSwitch));
}
