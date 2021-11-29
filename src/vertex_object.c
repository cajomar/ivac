#include "vertex_object.h"

#include "shader.h"

#include "assert.h"

static size_t size_of_gl_type(GLenum type) {
    switch (type) {
    case GL_FLOAT:
        return sizeof(float);
    default:
        FATAL_ERROR("unexpected enum value 0x%x\n", type);
        return 0;
    }
}

void vertex_object_init(VertexObject* vo,
                unsigned int num_attribs,
                GLenum* attrib_types,
                uint8_t* attrib_counts) {
    glGenVertexArrays(1, &vo->vao);
    glGenBuffers(1, &vo->vbo);

    glBindVertexArray(vo->vao);
    glBindBuffer(GL_ARRAY_BUFFER, vo->vbo);

    size_t stride = 0;
    for (int i = 0; i < num_attribs; i++) {
        stride += attrib_counts[i] * size_of_gl_type(attrib_types[i]);
    }

    size_t skip = 0;
    for (int i = 0; i < num_attribs; i++) {
        glEnableVertexAttribArray(i);
        glVertexAttribPointer(i, attrib_counts[i], attrib_types[i], GL_FALSE,
                              stride, (void*)skip);
        skip += attrib_counts[i] * size_of_gl_type(attrib_types[i]);
    }
    assert(skip == stride);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void vertex_object_deinit(VertexObject* vo) {
    glDeleteBuffers(1, &vo->vbo);
    glDeleteVertexArrays(1, &vo->vao);
}
