#ifndef RENDERER_H
#define RENDERER_H

#include <stdbool.h>

#include "app.h"
#include "editor.h"
#include "browser.h"
#include "filesystem.h"

bool renderer_init(void);

void renderer_render(AppContext *context);

void renderer_cursor_blink(void);

#endif