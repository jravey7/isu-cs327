#include <stdio.h>
#include <stdlib.h>

int hbarray[100000000] = {};

int main(int argc, char * argv[])
{
  int i;

  hbarray[0] = 1;
  printf("%d\n", hbarray[0]);

  //b(2n+1) = b(n) and b(2n + 2) = b(n) + b(n+1)   (n = 0,1,2...)
  for(i = 1; i < 100000000; i++) {

    if(i % 2 == 0) // even indexes
      hbarray[i] = hbarray[(i-2)/2] + hbarray[(i-2)/2 + 1];
    else // odd indexes
      hbarray[i] =  hbarray[(i - 1)/2];

    printf("%d\n", hbarray[i]);
  }
  
  
  return 0;
}
