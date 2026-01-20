cmake_minimum_required(VERSION 3.10)
project(CpuMulticoreSim C)

set(CMAKE_C_STANDARD 99)

# Include directories
include_directories(include)

# Define source files
file(GLOB_RECURSE SOURCES "src/*.c")

# Create executable
add_executable(sim_exe ${SOURCES})
add_executable(asm_exe assembler.c)