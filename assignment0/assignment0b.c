#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int nth_hyperbinary_number(int n)
{
  if(n == 0 || n == 1)
    return 1;
  if(n % 2 == 0)
    return nth_hyperbinary_number((n-2)/2) + nth_hyperbinary_number((n-2)/2 + 1);
  return nth_hyperbinary_number((n-1)/2);
}

int determine_last_digit(char * str)
{
  while(*(str + 1) != 0) 
    str++;  

  return atoi(str);
}

int determine_ends_with_tween(char * str)
{
  while(*(str + 2) != 0)
    str++;

  if(atoi(str) == 11 || atoi(str) == 12 || atoi(str) == 13)
    return 1;
  return 0;  
}

int main(int argc, char * argv[])
{
  if(argc != 2) {
    printf("Usage: assignment0b <nth rational number to print>\n");

    return -1;
  }

  char postfix[3] = {};
  int last_digit = determine_last_digit(argv[1]);


  if(determine_ends_with_tween(argv[1]))
     strcpy(postfix, "th");
  else
      switch(last_digit) {
        case 1: strcpy(postfix, "st");
	  break;
        case 2: strcpy(postfix, "nd");
	  break;
        case 3: strcpy(postfix, "rd");
	  break;
        default: strcpy(postfix, "th");
      }

 
    
  printf("The %d%s rational number is %d/%d\n", atoi(argv[1]), postfix, nth_hyperbinary_number(atoi(argv[1])), nth_hyperbinary_number(atoi(argv[1]) + 1));
  
  return 0;
}
