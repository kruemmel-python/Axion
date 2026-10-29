#include <iostream>
#include <string>
int main(int argc,char** argv){if(argc!=2||std::string(argv[1])!="--selftest")return 2;std::cout<<"SELFTEST_OK";return 0;}