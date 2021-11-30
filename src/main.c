#include "gl_core_4_3.h"
#include <GLFW/glfw3.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG
#include "stb_image.h"

#include "shader.h"
#include "vertex_object.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

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

double contrast = 0.5;
bool dragging_handle = false;

const float handle_size = 12;

typedef struct rect {
    float x, y, w, h;
} Rect;

static bool in_bounds(float x, float y, Rect* rect) {
    return !(x < rect->x || x > rect->x + rect->w || y < rect->y ||
             y > rect->y + rect->h);
}

static void pixel_to_gl_screen(float x, float y, float* _x, float* _y) {
    *_x = x / viewport[0] * 2 - 1;
    *_y = -(y / viewport[1] * 2 - 1);
}

static Rect get_slider_bounds() {
    const float slider_width = 8;
    const float padding = 24;
    const float slider_length = viewport[1] - padding * 2 - handle_size;

    Rect rect = {
        .x = viewport[0] - padding - slider_width,
        .w = slider_width,
        .y = padding,
        .h = slider_length,
    };
    return rect;
}

static Rect get_slider_gui_bounds() {
    Rect rect = get_slider_bounds();
    rect.y -= handle_size / 2;
    rect.h += handle_size;
    return rect;
}

static float get_handle_pos() {
    Rect slider = get_slider_bounds();
    return slider.y + slider.h * contrast;
}

static void set_handle_pos(float y) {
    Rect slider = get_slider_bounds();
    float handle_y = y - slider.y;
    if (handle_y <= 0) {
        handle_y = 1;
    } else if (handle_y >= slider.h) {
        handle_y = slider.h - 1;
    }
    contrast = handle_y / slider.h;
}

static Rect get_handle_bounds() {
    Rect slider = get_slider_bounds();
    float y = get_handle_pos();
    Rect rect = {
        .x = slider.x - (handle_size - slider.w) / 2,
        .w = handle_size,
        .y = y - handle_size / 2,
        .h = handle_size,
    };
    return rect;
}

static void window_resize_callback(GLFWwindow* window, int w, int h) {
    dirty = true;
    viewport[0] = w;
    viewport[1] = h;
    glViewport(0, 0, w, h);
}

static void scroll_callback(GLFWwindow* window, double x, double y) {
    dirty = true;
    float zoom_add = zoom * y * 0.3f;
    float cx, cy; // Relative to screen's center
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

static void
mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            Rect slider = get_slider_bounds();
            Rect handle = get_handle_bounds();
            if (in_bounds(cursor_x, cursor_y, &slider) ||
                in_bounds(cursor_x, cursor_x, &handle)) {
                dragging_handle = true;
                set_handle_pos(cursor_y);
                dirty = true;
            }
        } else if (action == GLFW_RELEASE) {
            dragging_handle = false;
        }
    }
}

static void mouse_motion_callback(GLFWwindow* window, double x, double y) {
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
        dirty = true;
        if (dragging_handle) {
            set_handle_pos(y);
        } else {
            float cx, cy, px, py;
            pixel_to_gl_screen(cursor_x, cursor_y, &cx, &cy);
            pixel_to_gl_screen(x, y, &px, &py);

            scroll_x += (px - cx) * 1;
            scroll_y += (py - cy) * 1;
        }
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

static GLuint get_slider_shader() {
    const char* const vertex_source = "#version 430 core\n"
                                      "in vec2 pos;\n"
                                      "void main() {\n"
                                      "    gl_Position = vec4(pos, 0.0, 1.0);\n"
                                      "}\n";

    // Be sure to update uniform setting when changing uniform positions
    const char* fragment_source = "#version 430 core\n"
                                  "out vec4 frag_color;\n"
                                  "uniform vec3 color;\n"
                                  "void main() {\n"
                                  "    frag_color = vec4(color, 1.0);\n"
                                  "}\n";

    return shader_new(vertex_source, fragment_source);
}

static GLuint get_image_shader() {
    const char* vertex_source = "#version 430 core\n"
                                "in vec2 pos;\n"
                                "in vec2 v_uv;\n"
                                "out vec2 uv;\n"
                                "void main() {\n"
                                "    uv = v_uv;\n"
                                "    gl_Position = vec4(pos, 0.0, 1.0);\n"
                                "}\n";

    // Be sure to update uniform setting when changing uniform positions
    const char* fragment_source =
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

static void build_image_buffer(int image_width, int image_height, GLuint vbo) {
    float verts[4][4] = {
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
        verts[i][0] *= zoom;
        verts[i][1] *= zoom;

        verts[i][0] += scroll_x;
        verts[i][1] += scroll_y;

        if (aspect_diff > 0) {
            verts[i][0] *= image_aspect / viewport_aspect;
        } else if (aspect_diff < 0) {
            verts[i][1] *= 1 / image_aspect * viewport_aspect;
        }
    }
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 16, verts, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

static void build_quad_buffer(GLuint vbo, Rect r) {
    float x1, x2, y1, y2;
    pixel_to_gl_screen(r.x, r.y, &x1, &y1);
    pixel_to_gl_screen(r.x + r.w, r.y + r.h, &x2, &y2);

    float verts[4][2] = {
        {x1, y1},
        {x2, y1},
        {x1, y2},
        {x2, y2},
    };
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 8, verts, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
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
    glfwSetMouseButtonCallback(win, mouse_button_callback);
    glfwSetScrollCallback(win, scroll_callback);

    printf("OpenGL %s GLSL %s\n", glGetString(GL_VERSION),
           glGetString(GL_SHADING_LANGUAGE_VERSION));

    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(message_callback, 0);

    glViewport(0, 0, viewport[0], viewport[1]);

    VertexObject image;
    {
        GLenum types[] = {
            GL_FLOAT,
            GL_FLOAT,
        };
        uint8_t counts[] = {
            2,
            2,
        };
        vertex_object_init(&image, 2, types, counts);
    }

    VertexObject slider;
    {
        GLenum types[] = {
            GL_FLOAT,
        };
        uint8_t counts[] = {
            2,
        };
        vertex_object_init(&slider, 1, types, counts);
    }

    GLuint image_shader = get_image_shader();
    GLuint slider_shader = get_slider_shader();

    GLuint tex = create_texture(w, h, c, data);
    stbi_image_free(data);

    glClearColor(0, 0, 0, 0);

    while (!glfwWindowShouldClose(win)) {
        glfwWaitEvents();
        if (dirty) {
            dirty = false;
            glClear(GL_COLOR_BUFFER_BIT);
            {
                glUseProgram(image_shader);
                // Note: Here I'm manually setting the uniform position. Be
                // sure to update when editing shaders!
                glUniform1f(1, 1 - logf(contrast * 2));
                glBindVertexArray(image.vao);
                build_image_buffer(w, h, image.vbo);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
                // glBindVertexArray(0);
            }
            {
                glUseProgram(slider_shader);
                // Note: Here I'm manually setting the uniform position. Be
                // sure to update when editing shaders!
                glUniform3f(0, 1.0, 0.8, 0.4);
                glBindVertexArray(slider.vao);
                build_quad_buffer(slider.vbo, get_slider_gui_bounds());
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
                // glBindVertexArray(0);

                glUniform3f(glGetUniformLocation(slider_shader, "color"), 0.4,
                            0.8, 1.0);
                build_quad_buffer(slider.vbo, get_handle_bounds());
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            }
            glfwSwapBuffers(win);
        }
    }

    glDeleteTextures(1, &tex);
    glDeleteProgram(image_shader);
    vertex_object_deinit(&image);
    vertex_object_deinit(&slider);

    return 0;
}
