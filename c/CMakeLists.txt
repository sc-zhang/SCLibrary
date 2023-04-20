cmake_minimum_required(VERSION 3.5)

project(demo LANGUAGES C)

set(CMAKE_C_STANDARD 11)

set(SOURCES
    demo.c)
set(HEADERS
    threadpool.h)

add_executable(demo ${SOURCES} ${HEADERS})
target_link_libraries(demo pthread)
