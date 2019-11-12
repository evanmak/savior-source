#include<stdlib.h>
#include<stdio.h>

#define MAX (256)
int main(int argc, char* argv[]) {
  char array[MAX-5]; // 0 < MAX < INT_MAX
  for(int i = 0; i < MAX;){
    array[i] = getchar();//LABEL: OOB access
    i++;//LABEL: integer-overflow
  }
  return 0;
}
