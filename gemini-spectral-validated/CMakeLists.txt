cmake_minimum_required(VERSION 3.20)
project(Signal42Explorer CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(research_candidate main.cpp)

if(MSVC)
    target_compile_options(research_candidate PRIVATE /W4 /WX)
else()
    target_compile_options(research_candidate PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
