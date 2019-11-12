#include<stdlib.h>
#include<stdio.h>

#define MAX (256)
int main(int argc, char* argv[]) {
  char array[MAX]; // 0 < MAX < INT_MAX
  int i = 512;
  if (i < MAX){
      array[i] = getchar();
  }else{
      i= 0xfffffffe | array[i];
  }

  return 0;
}
