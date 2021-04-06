#include <stdlib.h>
#include <stdio.h>

int exe() {
    printf("exe malloc\n");
    int i=100;
    char * buffer;

    buffer = (char*) malloc (i+1);
    if (buffer==NULL) exit (1);

    int n;
    for (n=0; n<i; n++)
        buffer[n]=rand()%26+'a';
    buffer[i]='\0';

  printf ("Random string: %s\n",buffer);
  free (buffer);
  return 0;
}

int main() {
    exe();
}