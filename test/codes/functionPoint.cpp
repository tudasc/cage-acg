// RUN: env GENCC_CG_NAME=%t.mcg %cage_cxx %s -o %t && %filecheck %s --input-file=%t.mcg

int foo(int) {
  return 5;
}

int (*fptr)(int);

int main() {
  int a = 5;
  fptr  = foo;
  return fptr(a);
}

// CHECK: "nodes":
// CHECK: "callees": {}
// CHECK: "functionName": "_Z3fooi"
// CHECK: "callees": {
// CHECK-NEXT: "0": {}
// CHECK-NEXT: }
// CHECK: "functionName": "main"
// CHECK: "localflow":
// CHECK-NEXT: "locals": [
// CHECK-NEXT: {
// CHECK-NEXT: "callees": [
// CHECK-NEXT: 0
// CHECK-NEXT: ],
// CHECK-NEXT: "loc": {
// CHECK-NEXT: "col": 10,
// CHECK-NEXT: "line": 12
// CHECK-NEXT: }
// CHECK-NEXT: }
// CHECK-NEXT: ]
