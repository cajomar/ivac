cmake_minimum_required(VERSION 3.13)
project(IVAC)

add_executable(ivac
    src/gl_core_4_3.c
    src/gui.c
    src/main.c
    src/shader.c
    src/vertex_object.c
    )
target_link_libraries(ivac glfw)

if (WIN32)
    target_link_libraries(ivac opengl32)
else()
    target_link_libraries(ivac m GL)
    target_compile_options(ivac PRIVATE -Wextra -Wall -pedantic -Wno-unused-parameter)
endif()
