#include "shader.h"

static GLuint compile_shader(GLenum type, const char* const source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        int len;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
        char* log = malloc(len);
        if (log == NULL) {
            FATAL_ERROR("malloc failed\n");
        } else {
            glGetShaderInfoLog(shader, len, NULL, log);
            FATAL_ERROR("failed to compile shader: %s\n", log);
            free(log);
        }
    }
    return shader;
}

GLuint shader_new(const char* const vertex_source,
                     const char* const fragment_source) {
    GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER, vertex_source);
    GLuint fragment_shader =
        compile_shader(GL_FRAGMENT_SHADER, fragment_source);

    GLuint program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);

    int success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        int len;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);
        char* log = malloc(len);
        if (log == NULL) {
            FATAL_ERROR("malloc failed\n");
        } else {
            glGetProgramInfoLog(program, len, NULL, log);
            FATAL_ERROR("failed link shader program: %s\n", log);
            free(log);
        }
    }
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    return program;
}
