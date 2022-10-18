cmake_minimum_required(VERSION 2.8)

project(RISC1RUN C)
add_executable(risc1run risc1run.c yaml.c Node.c NodeStack.c)
install(TARGETS risc1run DESTINATION bin)
