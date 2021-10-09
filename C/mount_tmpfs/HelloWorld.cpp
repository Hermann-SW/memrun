#!/usr/local/bin/g++ -run
#include <iostream>

int main()
{
  std::cout << "Hello, World!" << std::endl;

  for(char c; std::cin.read(&c, 1); )  { std::cout << c; }

  return 0;
}
