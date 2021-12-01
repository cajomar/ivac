#include "gl_core_4_3.h"
#include <GLFW/glfw3.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "gui.h"
#include "shader.h"
#include "vertex_object.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

// Width and height of the window
float viewport[2];
// If the window needs to be re-drawn
static bool dirty = true;
// Scale of the image
static float zoom = 1.0;
// Image center's offset
static float scroll_x = 0.0;
static float scroll_y = 0.0;
// Pixel location of mouse
static float cursor_x = 0.0;
static float cursor_y = 0.0;
// Value of the image slider form 0-1
float contrast = 0.5;
// If we're dragging the handle
static bool dragging_handle = false;
// If we should save the image
static bool save_image = false;

static void queue_save_image() { save_image = true; }
static void drag_handle() {
    dragging_handle = true;
    set_handle_pos(cursor_y);
    dirty = true;
}

struct widget {
    Rect (*get_bounds)();
    void (*mouse_button_down_callback)();
    float color[3];
};

#define NUM_WIDGETS 3
const struct widget widgets[NUM_WIDGETS] = {
    {get_save_button_bounds, queue_save_image, {0.4, 0.8, 1.0}},
    {get_slider_gui_bounds, drag_handle, {1.0, 0.8, 0.4}},
    {get_handle_bounds, drag_handle, {0.4, 0.8, 1.0}},
};

static void pixel_to_gl_screen(float x, float y, float* _x, float* _y) {
    *_x = x / viewport[0] * 2 - 1;
    *_y = -(y / viewport[1] * 2 - 1);
}

static void window_resize_callback(GLFWwindow* window, int w, int h) {
    dirty = true;
    viewport[0] = w;
    viewport[1] = h;
}

static void scroll_callback(GLFWwindow* window, double x, double y) {
    dirty = true;
    const float zoom_add = zoom * y * 0.3f;
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

static void mouse_button_callback(GLFWwindow* window, int button, int action,
                                  int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            for (int i = 0; i < NUM_WIDGETS; ++i) {
                Rect r = widgets[i].get_bounds();
                if (in_bounds(cursor_x, cursor_y, &r)) {
                    widgets[i].mouse_button_down_callback();
                    break;
                }
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
    FATAL_ERROR("GLFW Error %d: %s\n", code, description);
}

static void GLAPIENTRY message_callback(GLenum source, GLenum type, GLuint id,
                                        GLenum severity, GLsizei length,
                                        const GLchar* message,
                                        const void* userParam) {
    FATAL_ERROR(
        "GL CALLBACK: %s source = 0x%x, type = 0x%x, severity = 0x%x, "
        "message = %s\n",
        (type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : ""), source, type,
        severity, message);
}

static GLint bpp_to_gl_image_format(unsigned int bpp) {
    switch (bpp) {
    case 4: return GL_RGBA;
    case 3: return GL_RGB;
    case 2: return GL_RG;
    case 1: return GL_ALPHA;
    default: return GL_RGBA;
    }
}

static void build_first_image_buffer(GLuint vbo) {
    const float verts[4][4] = {
        // xyuv
        {-1, +1, 0, 1},
        {-1, -1, 0, 0},
        {+1, +1, 1, 1},
        {+1, -1, 1, 0},
    };
    GLDEBUG(glBindBuffer(GL_ARRAY_BUFFER, vbo));
    GLDEBUG(glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 16, verts,
                         GL_DYNAMIC_DRAW));
    GLDEBUG(glBindBuffer(GL_ARRAY_BUFFER, 0));
}

static void build_image_buffer(int image_width, int image_height, GLuint vbo) {
    float verts[4][4] = {
        // xyuv
        {-1, +1, 0, 1},
        {-1, -1, 0, 0},
        {+1, +1, 1, 1},
        {+1, -1, 1, 0},
    };
    const float image_aspect = image_width / (float)image_height;
    const float viewport_aspect = viewport[0] / viewport[1];
    const float aspect_diff = viewport_aspect - image_aspect;

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
    GLDEBUG(glBindBuffer(GL_ARRAY_BUFFER, vbo));
    GLDEBUG(glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 16, verts,
                         GL_DYNAMIC_DRAW));
    GLDEBUG(glBindBuffer(GL_ARRAY_BUFFER, 0));
}

static void build_quad_buffer(GLuint vbo, Rect r) {
    float x1, x2, y1, y2;
    pixel_to_gl_screen(r.x, r.y, &x1, &y1);
    pixel_to_gl_screen(r.x + r.w, r.y + r.h, &x2, &y2);

    const float verts[4][2] = {
        {x1, y1},
        {x2, y1},
        {x1, y2},
        {x2, y2},
    };
    GLDEBUG(glBindBuffer(GL_ARRAY_BUFFER, vbo));
    GLDEBUG(glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 8, verts,
                         GL_DYNAMIC_DRAW));
    GLDEBUG(glBindBuffer(GL_ARRAY_BUFFER, 0));
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

    if (image_height > mode->height || image_width > mode->width) {
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

int main(const int argc, const char* const* const argv) {
    if (argc != 2) {
        FATAL_ERROR("expected 1 argument, got %d\n", argc - 1);
        return -1;
    }

    stbi_set_flip_vertically_on_load(true);
    stbi_flip_vertically_on_write(true);
    int w, h, c;
    uint8_t* const data = stbi_load(argv[1], &w, &h, &c, 0);
    if (data == NULL) {
        FATAL_ERROR("failed to load %s: %s\n", argv[1], stbi_failure_reason());
        return -1;
    }

    GLFWwindow* const win = setup_glfw(w, h);
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

    GLDEBUG(glEnable(GL_DEBUG_OUTPUT));
    GLDEBUG(glDebugMessageCallback(message_callback, 0));

    VertexObject image, gui;
    {
        const GLenum types[2] = {GL_FLOAT, GL_FLOAT};
        const uint8_t counts[2] = {2, 2};
        vertex_object_init(&gui, 1, types, counts);
        vertex_object_init(&image, 2, types, counts);
    }

    const GLuint gui_shader = get_gui_shader();
    const GLuint image_shader = get_image_shader();
    const GLuint display_shader = get_display_shader();

    GLuint tex[2];
    GLDEBUG(glGenTextures(2, tex));

    // Set original image texture data
    GLDEBUG(glBindTexture(GL_TEXTURE_2D, tex[0]));
    GLDEBUG(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GLDEBUG(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    GLint fmt = bpp_to_gl_image_format(c);
    GLDEBUG(glTexImage2D(GL_TEXTURE_2D, 0, fmt, w, h, 0, fmt, GL_UNSIGNED_BYTE,
                         data));
    stbi_image_free(data);

    // Create the framebuffer object
    GLuint fbo;
    GLDEBUG(glGenFramebuffers(1, &fbo));
    GLDEBUG(glBindFramebuffer(GL_FRAMEBUFFER, fbo));
    GLDEBUG(glBindTexture(GL_TEXTURE_2D, tex[1]));
    GLDEBUG(glTexImage2D(GL_TEXTURE_2D, 0, fmt, w, h, 0, fmt, GL_UNSIGNED_BYTE,
                         NULL));
    GLDEBUG(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GLDEBUG(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    GLDEBUG(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                   GL_TEXTURE_2D, tex[1], 0));

    GLDEBUG(glReadBuffer(GL_COLOR_ATTACHMENT0));
    GLDEBUG(glDrawBuffer(GL_COLOR_ATTACHMENT0));

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        FATAL_ERROR("framebuffer is not complete\n");
        return -1;
    }

    GLDEBUG(glClearColor(0, 0, 0, 0));

    while (!glfwWindowShouldClose(win)) {
        glfwWaitEvents();
        if (dirty) {
            dirty = false;

            // First render image to framebuffer, adjusting the contrast
            GLDEBUG(glBindFramebuffer(GL_FRAMEBUFFER, fbo));
            GLDEBUG(glViewport(0, 0, w, h));
            GLDEBUG(glClear(GL_COLOR_BUFFER_BIT));

            GLDEBUG(glUseProgram(image_shader));
            GLDEBUG(glBindTexture(GL_TEXTURE_2D, tex[0]));
            // Note: Here I'm manually setting the uniform position. Be
            // sure to update when editing shaders!
            GLDEBUG(glUniform1f(1, 1 - logf(contrast * 2)));
            GLDEBUG(glBindVertexArray(image.vao));
            build_first_image_buffer(image.vbo);
            GLDEBUG(glDrawArrays(GL_TRIANGLE_STRIP, 0, 4));

            // Now render to screen
            GLDEBUG(glBindFramebuffer(GL_FRAMEBUFFER, 0));
            GLDEBUG(glViewport(0, 0, viewport[0], viewport[1]));
            GLDEBUG(glClear(GL_COLOR_BUFFER_BIT));

            // Render the edited image
            GLDEBUG(glUseProgram(display_shader));
            GLDEBUG(glBindTexture(GL_TEXTURE_2D, tex[1]));
            // The image vao is already bound
            build_image_buffer(w, h, image.vbo);
            GLDEBUG(glDrawArrays(GL_TRIANGLE_STRIP, 0, 4));

            // Render the slider
            GLDEBUG(glUseProgram(gui_shader));
            GLDEBUG(glBindVertexArray(gui.vao));
            for (int i = 0; i < NUM_WIDGETS; ++i) {
                // Note: Here I'm manually setting the uniform position. Be
                // sure to update when editing shader uniforms!
                GLDEBUG(glUniform3fv(0, 1, widgets[i].color));
                build_quad_buffer(gui.vbo, widgets[i].get_bounds());
                GLDEBUG(glDrawArrays(GL_TRIANGLE_STRIP, 0, 4));
            }
            glfwSwapBuffers(win);
        }
        if (save_image) {
            save_image = false;
            const size_t bufsize = w * h * c;
            uint8_t* data = malloc(bufsize);
            if (!data) {
                FATAL_ERROR("failed to allocate %zu bytes\n", bufsize);
                continue;
            }
            GLDEBUG(
                glGetTexImage(GL_TEXTURE_2D, 0, fmt, GL_UNSIGNED_BYTE, data));
            printf("Saving image to out.jpg\n");
            stbi_write_jpg("out.jpg", w, h, c, data, 100);
            free(data);
        }
    }

    GLDEBUG(glDeleteFramebuffers(1, &fbo));
    GLDEBUG(glDeleteTextures(2, tex));
    GLDEBUG(glDeleteProgram(gui_shader));
    GLDEBUG(glDeleteProgram(image_shader));
    GLDEBUG(glDeleteProgram(display_shader));
    vertex_object_deinit(&image);
    vertex_object_deinit(&gui);

    return 0;
}
