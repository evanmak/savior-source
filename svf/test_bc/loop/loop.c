#include <stdio.h>

int add(int a , int b){

	if(a > 5)
		return a+b; 

	else
		return a;
}


int main()
{ 
  int x = 0;  /* Don't forget to declare variables */
  int res = 0;  


  for(int j=0; j < 20; j++){
    while ( x < 10 ) { /* While x is less than 10 */
      res = add(x, res);

      printf( "%d\n", res);
      x++;             /* Update x so the condition can be met eventually */
    }
    if(j<x){printf("good\n");}
  }
  getchar();
}
