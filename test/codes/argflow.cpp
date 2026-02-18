// RUN: env GENCC_CG_NAME=%t.mcg %cage_cxx %s -o %t && %filecheck %s --input-file=%t.mcg

int __attribute__((used)) bar(int a, int b, const char** p) {
  return 0;
}

int foo(int a, int b, const char** p) {
  return 0;
}

int (*p)(int a, int b, const char** p) = bar;

int main(int argc, const char* argv[argc]) {
  int a;
  if (argc > 1)
    a = argc;
  else
    a = 1;

  int b;
  foo(a, argc, (const char**)&b);

  p(a, argc, (const char**)&b);

  return foo(0, argc, argv);
}

// CHECK: "nodes":
// CHECK: "argflow":
// CHECK-NEXT: "args": [
// CHECK-NEXT: {
// CHECK-NEXT: "idx": 0,
// CHECK-NEXT: "outs": [
// CHECK-NEXT: {
// CHECK-NEXT: "by_ref": false,
// CHECK-NEXT: "callees": [
// CHECK-NEXT: 1
// CHECK-NEXT: ],
// CHECK-NEXT: "idx": 0,
// CHECK-NEXT: "loc": {
// CHECK-NEXT: "col": 3,
// CHECK-NEXT: "line": 21
// CHECK-NEXT: }
// CHECK-NEXT: },
// CHECK-NEXT: {
// CHECK-NEXT: "by_ref": false,
// CHECK-NEXT: "callees": [
// CHECK-NEXT: 0,
// CHECK-NEXT: 1
// CHECK-NEXT: ],
// CHECK-NEXT: "idx": 0,
// CHECK-NEXT: "loc": {
// CHECK-NEXT: "col": 3,
// CHECK-NEXT: "line": 23
// CHECK-NEXT: }
// CHECK-NEXT: },
// CHECK-NEXT: {
// CHECK-NEXT: "by_ref": false,
// CHECK-NEXT: "callees": [
// CHECK-NEXT: 1
// CHECK-NEXT: ],
// CHECK-NEXT: "idx": 1,
// CHECK-NEXT: "loc": {
// CHECK-NEXT: "col": 10,
// CHECK-NEXT: "line": 25
// CHECK-NEXT: }
// CHECK-NEXT: }
// CHECK-NEXT: ]
// CHECK-NEXT: },
// CHECK-NEXT: {
// CHECK-NEXT: "idx": 1,
// CHECK-NEXT: "outs": [
// CHECK-NEXT: {
// CHECK-NEXT: "by_ref": true,
// CHECK-NEXT: "callees": [
// CHECK-NEXT: 1
// CHECK-NEXT: ],
// CHECK-NEXT: "idx": 2,
// CHECK-NEXT: "loc": {
// CHECK-NEXT: "col": 10,
// CHECK-NEXT: "line": 25
// CHECK-NEXT: }
// CHECK-NEXT: }
// CHECK-NEXT: ]
// CHECK-NEXT: }
// CHECK-NEXT: ]
// CHECK: "localflow":
// CHECK-NEXT: "locals": [
// CHECK-NEXT: {
// CHECK-NEXT: "callees": [
// CHECK-NEXT: 0,
// CHECK-NEXT: 1
// CHECK-NEXT: ],
// CHECK-NEXT: "loc": {
// CHECK-NEXT: "col": 3,
// CHECK-NEXT: "line": 23
// CHECK-NEXT: }
// CHECK-NEXT: },
// CHECK-NEXT: {
// CHECK-NEXT: "callees": [
// CHECK-NEXT: 0,
// CHECK-NEXT: 1
// CHECK-NEXT: ],
// CHECK-NEXT: "loc": {
// CHECK-NEXT: "col": 3,
// CHECK-NEXT: "line": 23
// CHECK-NEXT: }
// CHECK-NEXT: }
// CHECK-NEXT: ]
