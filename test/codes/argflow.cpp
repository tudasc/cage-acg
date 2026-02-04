// RUN: env GENCC_CG_NAME=%t.mcg %cage_cxx %s -o %t && %filecheck %s --input-file=%t.mcg
// CHECK: "nodes":

int __attribute__((used)) bar(int a, int b, const char** p) {
  return 0;
}

int foo(int a, int b, const char** p) { return 0; }

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
