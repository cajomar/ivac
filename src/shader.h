#ifndef IVAC_SRC_SHADER_H_NGAJOF2E
#define IVAC_SRC_SHADER_H_NGAJOF2E

#include "gl_core_4_3.h"

#include <stdio.h>
#include <stdlib.h>

#define FATAL_ERROR(...) fprintf(stderr, "Error: " __VA_ARGS__)

GLuint shader_new(const char* const vertex_source,
                     const char* const fragment_source);

#endif /* IVAC_SRC_SHADER_H_NGAJOF2E */
