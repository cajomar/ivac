cmake_minimum_required(VERSION 3.13)
project(IVAC)

add_executable(ivac src/main.c src/gl_core_4_3.c)
target_link_libraries(ivac glfw)

if (WIN32)
    target_link_libraries(ivac opengl32)
else()
    target_link_libraries(ivac m GL)
endif()
