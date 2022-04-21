#include <stdio.h>

struct container{
  virtual int bar() {return -1;}
};

struct Base0: public virtual container{
  int a;
  virtual int bar() {return 1;}
  virtual int foo() {return 2;}
};


struct Ba_se1: public virtual container{
  int notA;
  long B;
  virtual int bar() {return 10;}
  virtual int meh() {return 30;}
};

struct Derived : public Base0, public Ba_se1 {
  virtual int bar() {return 100;}
  virtual int foo() {return 200;}
  virtual int meh() {return 300;}
  
};


int main(int argc, char** argv){
  int ifSwitch;
  scanf("%d",&ifSwitch);  

  container* c=0;

  if(ifSwitch==0){
    c= new Base0();
  }else if(ifSwitch==1){
    c= new Ba_se1();
  }else{
    c= new Derived();
  }

  printf("%d\n",c->bar());
  

}
