cmake_minimum_required(VERSION 3.13)
project(IVAC)

add_executable(ivac src/main.c src/gl_core_4_3.c)
target_link_libraries(ivac GL glfw)

if (NOT WIN32)
    target_link_libraries(ivac m)
endif()
