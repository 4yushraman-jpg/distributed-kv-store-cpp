cmake_minimum_required(VERSION 3.10)
project(RedisLite VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Find the standard Threads library (pthread)
find_package(Threads REQUIRED)

add_executable(server src/main.cpp)

# Link the Threads library to your server
target_link_libraries(server PRIVATE Threads::Threads)
target_compile_options(server PRIVATE -Wall -Wextra)