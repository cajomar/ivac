#include "gui.h"

extern const float viewport[2];
extern float contrast;

static const float handle_size = 12;

bool in_bounds(float x, float y, Rect* rect) {
    return !(x < rect->x || x > rect->x + rect->w || y < rect->y ||
             y > rect->y + rect->h);
}

Rect get_save_button_bounds() {
    Rect rect = {
        .x = 16,
        .w = 32,
        .y = 16,
        .h = 32,
    };
    return rect;
}

Rect get_slider_bounds() {
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

Rect get_slider_gui_bounds() {
    Rect rect = get_slider_bounds();
    rect.y -= handle_size / 2;
    rect.h += handle_size;
    return rect;
}

float get_handle_pos() {
    Rect slider = get_slider_bounds();
    return slider.y + slider.h * contrast;
}

void set_handle_pos(float y) {
    Rect slider = get_slider_bounds();
    float handle_y = y - slider.y;
    if (handle_y <= 0) {
        handle_y = 1;
    } else if (handle_y >= slider.h) {
        handle_y = slider.h - 1;
    }
    contrast = handle_y / slider.h;
}

Rect get_handle_bounds() {
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
