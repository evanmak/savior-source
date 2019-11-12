#include<stdlib.h>
#include<stdio.h>


void callee(){

    if(2+1>4){
        printf("ssss");
    }

}

#define MAX (256)
int main(int argc, char* argv[]) {
  char array[MAX]; // 0 < MAX < INT_MAX
  int i = 0;
  while(getchar()){
      for(;i< MAX; ++i) {
          for(int j = 0;i< MAX -2; ++j) {
              array[i+j] = getchar();
          }
      }
      callee();
  }

  return 0;
}
