#include <stdio.h>

struct Base {
  Base(){
    printf("Call to Base Constructor\n");
  }
  
  virtual ~Base(){
    printf("Call to Base Destructor\n");
  }
  virtual int bar() {return 2;}
  virtual int foo() {return 3;}
  int meh() {return 1;}
};

struct Derived : public Base {
  Derived(){
    printf("Call to Derived Constructor\n");
  }

  virtual ~Derived(){
    printf("Call to Derived Destructor\n");
  }

  virtual int bar() { return 20; } // some value
  virtual int foo() final {return 30;}
  int meh() {return 10;}
};

int add(Base *obj, int v) { return obj->bar() + v; }

int main(int argc, char** argv){
  int ifSwitch;
  scanf("%d",&ifSwitch);  

  Base* b=0;

  if(ifSwitch<5){
      b= new Derived();
  }else{
      b= new Base();
  }

  printf("%d\n",b->meh());

  delete b;
}
