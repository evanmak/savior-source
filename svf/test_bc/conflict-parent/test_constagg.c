#include<stdlib.h>
#include<stdio.h>

struct A {
    int a;
    char b;
    int c;
};

#define MAX (256)
int main(int argc, char* argv[]) {
  char array[MAX]; // 0 < MAX < INT_MAX
  struct A a = {.a = 1, .b='c',.c=2};
  int i = 512;
  if (i < MAX){
      array[i] = getchar();
  }else{
      i= a.c + 1;
  }

  return 0;
}
