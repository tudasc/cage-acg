// RUN: env GENCC_CG_NAME=%t.mcg %cage_cxx %s -o %t && %filecheck %s --input-file=%t.mcg
// CHECK: "nodes":

int foo() {
  return 5;
}

int main() {
  foo();
  return 0;
}
