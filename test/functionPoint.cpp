int foo(int){
  return 5;
}

int (*fptr)(int);

int main(){
  fptr=foo;
  return fptr(5);
}
