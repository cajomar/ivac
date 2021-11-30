#ifndef IVAC_SRC_SHADER_H_NGAJOF2E
#define IVAC_SRC_SHADER_H_NGAJOF2E

#include "gl_core_4_3.h"

#include <stdio.h>
#include <stdlib.h>

#define FATAL_ERROR(...) fprintf(stderr, "Error: " __VA_ARGS__)

#if 1
#define GLDEBUG(x)                                                             \
    x;                                                                         \
    {                                                                          \
        GLenum e;                                                              \
        if ((e = glGetError()) != GL_NO_ERROR) {                               \
            printf("glError 0x%x at %s line %d\n", e, __FILE__, __LINE__);     \
            assert(false);                                                     \
        }                                                                      \
    }
#else
#define GLDEBUG(x) (x)
#endif

GLuint shader_new(const char* const vertex_source,
                  const char* const fragment_source);

GLuint get_gui_shader(void);
GLuint get_image_shader(void);
GLuint get_display_shader(void);

#endif /* IVAC_SRC_SHADER_H_NGAJOF2E */
