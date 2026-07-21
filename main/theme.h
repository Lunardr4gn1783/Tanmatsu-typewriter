#ifndef THEME_H
#define THEME_H

#include <stdint.h>

/* Number of available themes */
#define THEME_COUNT 20

/* Semantic color slots used by the renderer (ARGB8888) */
typedef struct
{
    const char *name;      // Shown in the menu ("Dark", "Paper", ...)

    uint32_t bg;           // Main background
    uint32_t fg;           // Main text
    uint32_t dim;          // De-emphasized text (paths, hints)
    uint32_t accent;       // Header / title text
    uint32_t highlight;    // Selection bar in menus and browser
    uint32_t header_bg;    // Browser header and help bar background
    uint32_t status_bg;    // Status bar background
    uint32_t status_fg;    // Status bar text
    uint32_t row1;         // Browser row background (odd)
    uint32_t row2;         // Browser row background (even)

} Theme;

/* Returns the theme for the given index (clamped to a valid range) */
const Theme *theme_get(int index);

/* Returns the display name for the given theme index */
const char *theme_name(int index);

#endif
