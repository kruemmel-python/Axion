#include "k4d.hpp"
#include <cstdint>
#include <iostream>
#include <string>
int main(int argc,char** argv){try{if(argc>1&&std::string(argv[1])=="selftest")return mre::k4d::selftest();return mre::k4d::run(argc>1?argv[1]:"k4d_result.json",argc>2?std::stoi(argv[2]):8,argc>3?std::stoi(argv[3]):100,argc>4?std::stoull(argv[4]):314159ULL);}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
