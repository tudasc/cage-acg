int __attribute__((used)) bar(int a, int b, const char** p) {
  return 0;
}

int foo(int a, int b, const char** p);

extern int (*p)(int a, int b, const char** p);

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
