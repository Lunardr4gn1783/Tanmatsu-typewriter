#ifndef RENDERER_H
#define RENDERER_H

#include <stdbool.h>

// Include the state modules so the renderer knows the struct layouts
#include "app.h"
#include "editor.h"
#include "browser.h"
#include "filesystem.h"

bool renderer_init(void);
void renderer_begin(void);
void renderer_present(void);

// Pass the specific sub-structs to keep the drawing functions modular
void renderer_draw_editor(const Editor *editor);
void renderer_draw_browser(const Browser *browser, const Filesystem *fs);
void renderer_draw_menu(int menu_selected); 

// The master render function now takes the whole context pointer
void renderer_render(AppContext *context);

void renderer_cursor_blink(void);

#endif