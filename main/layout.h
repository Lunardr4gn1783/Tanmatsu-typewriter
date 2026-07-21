#ifndef LAYOUT_H
#define LAYOUT_H

#include <stdbool.h>
#include <stddef.h>

#include "editor.h"

/* Maximum number of visual lines we track for one buffer. The editor
 * buffer is 2 KB, so even at ~10 chars per wrapped line this is plenty. */
#define LAYOUT_MAX_LINES 256

/* Scratch buffer size for measuring/drawing a single visual line.
 * Large enough for a full unwrapped buffer line. */
#define LAYOUT_MEASURE_MAX 512

/* One visual line: a half-open range [start, end) into editor->text.
 * For a hard-wrapped line, end points at the '\n'; for a soft wrap it
 * points just past the last character that fit. */
typedef struct
{
    size_t start;
    size_t end;
    bool   hard_break;  // true when this line ends at a real '\n'
} LayoutLine;

typedef struct
{
    LayoutLine lines[LAYOUT_MAX_LINES];
    int        count;
} Layout;

/* Recomputes the visual line breaks for the given editor buffer.
 *
 * When wrap is false, lines break only at '\n' (classic behavior; long
 * lines run off to the right and are scrolled horizontally).
 *
 * When wrap is true, lines also break at word boundaries so that no line
 * exceeds max_width pixels. measure() is used to size candidate strings,
 * so it must match whatever font and size the renderer will draw with.
 */
void layout_build(
    Layout *layout,
    const Editor *editor,
    bool wrap,
    int max_width,
    int (*measure)(const char *text, size_t length));

/* Returns the index of the visual line containing the given buffer
 * position, or 0 when the layout is empty. */
int layout_line_at(const Layout *layout, size_t position);

#endif
