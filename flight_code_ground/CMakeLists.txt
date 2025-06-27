cmake_minimum_required(VERSION 3.31)
project(flight_code_ground C)

set(CMAKE_C_STANDARD 11)

add_executable(flight_code_3 cli_ground.c
        cli_ground.h
        main.c)
