#include "gl_core_4_3.h"
#include <GLFW/glfw3.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG
#include "stb_image.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define FATAL_ERROR(...) fprintf(stderr, "Error: " __VA_ARGS__)

double viewport[2];
bool dirty = true;
// Scale of the image
double zoom = 1.0;
// Image center's offset
double scroll_x = 0.0;
double scroll_y = 0.0;
// Pixel location of mouse
double cursor_x = 0.0;
double cursor_y = 0.0;

static void pixel_to_gl_screen(double x, double y, double* _x, double* _y) {
    *_x = x / viewport[0] * 2 - 1;
    *_y = -(y / viewport[1] * 2 - 1);
}

static void window_resize_callback(GLFWwindow* window, int w, int h) {
    dirty = true;
    viewport[0] = w;
    viewport[1] = h;
    glViewport(0, 0, w, h);
}

static void scroll_callback(GLFWwindow* window, double x, double y) {
    dirty = true;
    double zoom_add = zoom * y * 0.3f;
    double cx, cy; // Relative to screen's center
    pixel_to_gl_screen(cursor_x, cursor_y, &cx, &cy);
    // Now relative to scroll
    cx -= scroll_x;
    cy -= scroll_y;

    // offset / zoom_add == c / zoom
    // offset            == c / zoom * zoom_add
    scroll_x -= cx / zoom * zoom_add;
    scroll_y -= cy / zoom * zoom_add;

    zoom += zoom_add;
    if (zoom < 0.01) {
        zoom = 0.01;
    }
}

static void mouse_motion_callback(GLFWwindow* window, double x, double y) {
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
        dirty = true;

        double cx, cy, px, py;
        pixel_to_gl_screen(cursor_x, cursor_y, &cx, &cy);
        pixel_to_gl_screen(x, y, &px, &py);

        scroll_x += (px - cx) * 1;
        scroll_y += (py - cy) * 1;
    }
    cursor_x = x;
    cursor_y = y;
}

static void error_callback(int code, const char* description) {
    fprintf(stderr, "GLFW Error %d: %s\n", code, description);
}

static void GLAPIENTRY message_callback(GLenum source,
                                        GLenum type,
                                        GLuint id,
                                        GLenum severity,
                                        GLsizei length,
                                        const GLchar* message,
                                        const void* userParam) {
    fprintf(stderr,
            "GL CALLBACK: %s source = 0x%x, type = 0x%x, severity = 0x%x, "
            "message = %s\n",
            (type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : ""), source, type,
            severity, message);
}

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

static GLuint get_shader() {
    const char* const vertex_source = "#version 430 core\n"
                                      "in vec2 pos;\n"
                                      "in vec2 v_uv;\n"
                                      "out vec2 uv;\n"
                                      "void main() {\n"
                                      "    uv = v_uv;\n"
                                      "    gl_Position = vec4(pos, 0.0, 1.0);\n"
                                      "}\n";

    GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER, vertex_source);

    const char* fragment_source = "#version 430 core\n"
                                  "in vec2 uv;\n"
                                  "out vec4 frag_color;\n"
                                  "uniform sampler2D tex;\n"
                                  "void main() {\n"
                                  "    frag_color = texture(tex, uv);\n"
                                  "}\n";
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

static GLint bpp_to_gl_image_format(unsigned int bpp) {
    switch (bpp) {
    case 4:
        return GL_RGBA;
    case 3:
        return GL_RGB;
    case 2:
        return GL_RG;
    case 1:
        return GL_ALPHA;
    default:
        return GL_RGBA;
    }
}

static GLuint create_texture(unsigned int w,
                             unsigned int h,
                             unsigned int c,
                             const uint8_t* data) {

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    GLint fmt = bpp_to_gl_image_format(c);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    if (data) {
        glTexImage2D(GL_TEXTURE_2D, 0, fmt, w, h, 0, fmt, GL_UNSIGNED_BYTE,
                     data);
    }
    return tex;
}

static void gen_quad(int image_width, int image_height) {
    float quad_verts[4][4] = {
        // xyuv
        {-1, +1, 0, 1},
        {-1, -1, 0, 0},
        {+1, +1, 1, 1},
        {+1, -1, 1, 0},
    };
    const double image_aspect = image_width / (double)image_height;
    const double viewport_aspect = viewport[0] / viewport[1];
    const double aspect_diff = viewport_aspect - image_aspect;

    for (int i = 0; i < 4; ++i) {
        quad_verts[i][0] *= zoom;
        quad_verts[i][1] *= zoom;

        quad_verts[i][0] += scroll_x;
        quad_verts[i][1] += scroll_y;
    }
    if (aspect_diff > 0) {
        for (int i = 0; i < 4; ++i) {
            quad_verts[i][0] *= image_aspect / viewport_aspect;
        }
    } else if (aspect_diff < 0) {
        for (int i = 0; i < 4; ++i) {
            quad_verts[i][1] *= 1 / image_aspect * viewport_aspect;
        }
    }
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 16, quad_verts,
                 GL_DYNAMIC_DRAW);
}

static GLFWwindow* setup_glfw(int image_width, int image_height) {
    glfwSetErrorCallback(error_callback);
    if (glfwInit() != GLFW_TRUE) {
        FATAL_ERROR("failed to initalize GLFW\n");
        return 0;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());

    if (image_height > mode->height) {
        viewport[0] = mode->width;
        viewport[1] = mode->height;
    } else if (image_width > mode->width) {
        viewport[0] = mode->width;
        viewport[1] = mode->height;
    } else {
        viewport[0] = image_width;
        viewport[1] = image_height;
    }

    GLFWwindow* window =
        glfwCreateWindow(viewport[0], viewport[1],
                         "Image Viewer Application Challenge", NULL, NULL);
    if (window == NULL) {
        FATAL_ERROR("failed in create glfw window\n");
        glfwTerminate();
        return 0;
    }
    glfwMakeContextCurrent(window);
    return window;
}

int main(int argc, const char** argv) {
    if (argc != 2) {
        FATAL_ERROR("expected 1 argument, got %d\n", argc - 1);
        return -1;
    }

    stbi_set_flip_vertically_on_load(true);
    int w, h, c;
    uint8_t* data = stbi_load(argv[1], &w, &h, &c, 0);
    if (data == NULL) {
        FATAL_ERROR("failed to load %s: %s\n", argv[1], stbi_failure_reason());
        return -1;
    }

    GLFWwindow* win = setup_glfw(w, h);
    if (win == NULL) {
        stbi_image_free(data);
        return -1;
    }

    glfwSetCursorPosCallback(win, mouse_motion_callback);
    glfwSetFramebufferSizeCallback(win, window_resize_callback);
    glfwSetScrollCallback(win, scroll_callback);

    printf("OpenGL %s GLSL %s\n", glGetString(GL_VERSION),
           glGetString(GL_SHADING_LANGUAGE_VERSION));

    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(message_callback, 0);

    glViewport(0, 0, viewport[0], viewport[1]);

    GLuint vao;
    GLuint vbo;
    {
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vao);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                              (void*)0);

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                              (void*)(2 * sizeof(float)));
    }

    GLuint shader = get_shader();
    glUseProgram(shader);

    GLuint tex = create_texture(w, h, c, data);
    stbi_image_free(data);

    glClearColor(0, 0, 0, 0);

    while (!glfwWindowShouldClose(win)) {
        glfwWaitEvents();
        if (dirty) {
            dirty = false;
            glClear(GL_COLOR_BUFFER_BIT);
            {
                glBindVertexArray(vao);
                gen_quad(w, h);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            }
            glfwSwapBuffers(win);
        }
    }

    glDeleteTextures(1, &tex);
    glDeleteProgram(shader);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);

    return 0;
}
