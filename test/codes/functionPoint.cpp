// RUN: env GENCC_CG_NAME=%t.mcg %cage_cxx %s -o %t && %filecheck %s --input-file=%t.mcg
// CHECK: "nodes":

int foo(int) {
  return 5;
}

int (*fptr)(int);

int main() {
  fptr = foo;
  return fptr(5);
}
