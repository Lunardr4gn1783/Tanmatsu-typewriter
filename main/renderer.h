#ifndef RENDERER_H
#define RENDERER_H

#include <stdbool.h>

#include "app.h"
#include "editor.h"
#include "browser.h"
#include "filesystem.h"
#include "layout.h"

#define EDITOR_FONT_COUNT 5

bool renderer_init(void);

void renderer_render(AppContext *context);

void renderer_cursor_blink(void);

void renderer_rebuild_layout(const Editor *editor);

const Layout *renderer_layout(void);

const char *renderer_font_name(int index);

#endif