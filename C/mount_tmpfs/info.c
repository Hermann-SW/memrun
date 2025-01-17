#!/usr/local/bin/gcc -run
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv)
{
  printf("My process ID : %d\n", getpid());
  printf("argv[0] : %s\n", argv[0]);
  printf(argc>1 ? "argv[1] : %s\n" : "no argv[1]", argv[1]);
  char buf[100];
  sprintf(buf, "/proc/%d/fd/", getpid());
  *strstr(argv[0], "/doit")=0;
  char *args[5]={"/usr/bin/ls", "-al", buf, argv[0], NULL};
  printf("\nevecve --> %s %s %s %s\n", args[0], args[1], args[2], args[3]);
  execve(args[0], args, args+3);
  return 0;
}
