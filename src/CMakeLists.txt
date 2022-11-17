cmake_minimum_required(VERSION 2.8)

project(RISC1RUN C)

add_library(NYaml STATIC yaml.c Node.c NodeStack.c)

add_library(risc1 STATIC risc1lib.c)

add_library(elcore STATIC elcorelib.c)

add_executable(risc1run risc1run.c)
target_link_libraries(risc1run PRIVATE NYaml risc1)

install(TARGETS risc1run DESTINATION bin)
