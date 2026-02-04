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