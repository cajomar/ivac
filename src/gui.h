#ifndef IVAC_SRC_GUI_H_9KZ8HBVG
#define IVAC_SRC_GUI_H_9KZ8HBVG

#include <stdbool.h>

typedef struct rect {
    float x, y, w, h;
} Rect;

float get_handle_pos(void);
void set_handle_pos(float y);
Rect get_slider_bounds(void);
Rect get_handle_bounds(void);
Rect get_slider_gui_bounds(void);
Rect get_save_button_bounds(void);
bool in_bounds(float x, float y, Rect* rect);

#endif /* IVAC_SRC_GUI_H_9KZ8HBVG */
