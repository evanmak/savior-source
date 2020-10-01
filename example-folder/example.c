#include <stdio.h>
#include <unistd.h>

long AFL_STOP = 0x41464c5f53544f50;

int main(int argc, char *argv[]) {
 int a,nb;
 long magic;
 
 nb = read(0, &magic, sizeof(long));
 if (nb == 0) return 1;
 if (nb < sizeof(long)) return 1; //add coverage if good size

 if (magic != AFL_STOP) return 1;

 puts("Magic number passed"); 
 
 nb = read(0, &a, sizeof(int));
 a = a - 12;
 
 return 0; 
}

