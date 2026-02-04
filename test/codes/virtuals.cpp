// RUN: env GENCC_CG_NAME=%t.mcg %cage_cxx %s -o %t && %filecheck %s --input-file=%t.mcg
// CHECK: "nodes":

struct Base {
  virtual int foo() {
    return 5;
  };
};

struct Derived : public Base {
  virtual int foo() {
    return 6;
  }
};

struct Unassociated {
  virtual int foo() {
    return 7;
  }
};

int bar(Base* pointer) {
  // we could call Derived::foo or Base::foo depending on the dynamic type.
  return pointer->foo();
  // The analysis should figure out that: *pointer could be of type Base or Derived but never of type Unassociated
}

int main() {
  Derived d;
  return bar(&d);
}