#ifndef IVAC_SRC_VAO_H_HJKNW8LT
#define IVAC_SRC_VAO_H_HJKNW8LT

#include "gl_core_4_3.h"

typedef struct vertex_object {
    GLuint vao;
    GLuint vbo;
} VertexObject;

void vertex_object_init(VertexObject* vo,
                        unsigned int num_attribs,
                        GLenum* attrib_types,
                        uint8_t* attrib_counts);

void vertex_object_deinit(VertexObject* vo);
#endif /* IVAC_SRC_VAO_H_HJKNW8LT */
