#include<stdlib.h>
#include<stdio.h>

#define MAX (256)
int main(int argc, char* argv[]) {
  char array[MAX]; // 0 < MAX < INT_MAX
  int i = 512;
  switch(getchar()){
  case '1':
      array[i] = getchar();
      break;
  case '2':
      i= 0xfffffffe | array[i];
  default:
      i++;
  }

  return 0;
}
