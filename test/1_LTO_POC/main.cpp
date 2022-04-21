#include <stdio.h>
#include "getString.h"

struct {
  int mitInt=4;
  char* undString="HierEinFunktionsname";
} wieSiehtEinStructAus;


int main(){
  int selector;
  scanf("%d",&selector);
  if(selector)
    printf("String gotten: %s,%d\n", getString(),wieSiehtEinStructAus.mitInt);
  else
    printf("String gotten: %s,%s\n", getString(),wieSiehtEinStructAus.undString);
}
