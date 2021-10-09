#!/bin/bash

echo "foo"

IFS=''
while read -r line
do
  echo "C: $line" 
done < <(g++ -run <(sed -n "/^\/\*\*$/,\$p" $0) 42)

echo "bar"

exit
/**
*/
#include <iostream>

int main(int argc, char *argv[])
{
  printf("bar %s\n", argc>1 ? argv[1] : "(undef)");

  for(char c; std::cin.read(&c, 1); )  { std::cout << c; }

  return 0;
}
