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

GLuint get_gui_shader() {
    const char* const vertex_source =
        "#version 430 core\n"
        "in vec2 pos;\n"
        "void main() {\n"
        "    gl_Position = vec4(pos, 0.0, 1.0);\n"
        "}\n";

    // Be sure to update uniform setting when changing uniform positions
    const char* const fragment_source =
        "#version 430 core\n"
        "out vec4 frag_color;\n"
        "uniform vec3 color;\n"
        "void main() {\n"
        "    frag_color = vec4(color, 1.0);\n"
        "}\n";

    return shader_new(vertex_source, fragment_source);
}

GLuint get_image_shader() {
    const char* const vertex_source =
        "#version 430 core\n"
        "in vec2 pos;\n"
        "in vec2 v_uv;\n"
        "out vec2 uv;\n"
        "void main() {\n"
        "    uv = v_uv;\n"
        "    gl_Position = vec4(pos, 0.0, 1.0);\n"
        "}\n";

    // Be sure to update uniform setting when changing uniform positions
    const char* const fragment_source =
        "#version 430 core\n"
        "in vec2 uv;\n"
        "out vec4 frag_color;\n"
        "uniform sampler2D tex;\n"
        "uniform float contrast;\n"
        "vec4 average_luminance = vec4(0.5, 0.5, 0.5, 1.0);\n"
        "void main() {\n"
        "    vec4 tex_color = texture(tex, uv);\n"
        "    frag_color = mix(average_luminance, tex_color, contrast);\n"
        "}\n";

    return shader_new(vertex_source, fragment_source);
}

GLuint get_display_shader() {
    const char* const vertex_source =
        "#version 430 core\n"
        "in vec2 pos;\n"
        "in vec2 v_uv;\n"
        "out vec2 uv;\n"
        "void main() {\n"
        "    uv = v_uv;\n"
        "    gl_Position = vec4(pos, 0.0, 1.0);\n"
        "}\n";

    // Be sure to update uniform setting when changing uniform positions
    const char* const fragment_source =
        "#version 430 core\n"
        "in vec2 uv;\n"
        "out vec4 frag_color;\n"
        "uniform sampler2D tex;\n"
        "void main() {\n"
        "    frag_color = texture(tex, uv);\n"
        "}\n";

    return shader_new(vertex_source, fragment_source);
}
