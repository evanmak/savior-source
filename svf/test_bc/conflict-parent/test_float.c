#include<stdlib.h>
#include<stdio.h>

struct MySt {
    int i;
    int j;
};

#define MAX (256)
int main(int argc, char* argv[]) {
  char array[MAX]; // 0 < MAX < INT_MAX
  int i = 512;
  double z = 512;
  int j = 123;
  if (j + i > -1) {
      if (i > -1){
          array[(int)i] = getchar();
      }else{
          i = 123 + (int)j;
      }
  }
  return 0;
}
