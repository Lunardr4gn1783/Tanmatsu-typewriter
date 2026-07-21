#include "layout.h"

#include <string.h>

static void layout_push(Layout *layout, size_t start, size_t end, bool hard)
{
    if (layout->count >= LAYOUT_MAX_LINES)
    {
        return;
    }

    layout->lines[layout->count].start      = start;
    layout->lines[layout->count].end        = end;
    layout->lines[layout->count].hard_break = hard;
    layout->count++;
}

void layout_build(
    Layout *layout,
    const Editor *editor,
    bool wrap,
    int max_width,
    int (*measure)(const char *text, size_t length))
{
    layout->count = 0;

    if (editor == NULL)
    {
        return;
    }

    size_t pos = 0;

    while (pos <= editor->length)
    {
        /* Find the end of this hard line (the next '\n' or end of text) */
        size_t hard_end = pos;
        while (hard_end < editor->length && editor->text[hard_end] != '\n')
        {
            hard_end++;
        }

        if (!wrap || measure == NULL || max_width <= 0)
        {
            layout_push(layout, pos, hard_end, hard_end < editor->length);
        }
        else
        {
            /* Break this hard line into as many visual lines as needed */
            size_t line_start = pos;

            while (line_start <= hard_end)
            {
                /* Grow the line one character at a time, remembering the
                 * last position where a break would be legal (after a
                 * space). This keeps whole words intact. */
                size_t last_space = 0;
                size_t i          = line_start;

                while (i < hard_end)
                {
                    size_t candidate = (i - line_start) + 1;

                    if (measure(editor->text + line_start, candidate) > max_width)
                    {
                        break;
                    }

                    if (editor->text[i] == ' ')
                    {
                        last_space = i;
                    }

                    i++;
                }

                if (i >= hard_end)
                {
                    /* The remainder fits on one line */
                    layout_push(layout, line_start, hard_end,
                                hard_end < editor->length);
                    line_start = hard_end + 1;
                    break;
                }

                /* Doesn't fit: prefer breaking after the last space so we
                 * don't split a word. Fall back to a hard character break
                 * for a single word longer than the screen. */
                size_t break_at;

                if (last_space > line_start)
                {
                    break_at = last_space + 1;  // keep the space on this line
                }
                else
                {
                    break_at = (i > line_start) ? i : line_start + 1;
                }

                layout_push(layout, line_start, break_at, false);
                line_start = break_at;

                if (layout->count >= LAYOUT_MAX_LINES)
                {
                    return;
                }
            }

            if (line_start <= hard_end)
            {
                pos = hard_end + 1;
                continue;
            }
        }

        if (hard_end >= editor->length)
        {
            break;
        }

        pos = hard_end + 1;
    }

    /* Always have at least one line so the cursor has somewhere to live */
    if (layout->count == 0)
    {
        layout_push(layout, 0, 0, false);
    }
}

int layout_line_at(const Layout *layout, size_t position)
{
    for (int i = 0; i < layout->count; i++)
    {
        if (position >= layout->lines[i].start && position <= layout->lines[i].end)
        {
            /* A position sitting exactly on a soft break belongs to the
             * next line, so the cursor appears where typing continues. */
            if (position == layout->lines[i].end &&
                !layout->lines[i].hard_break &&
                i + 1 < layout->count &&
                layout->lines[i + 1].start == position)
            {
                continue;
            }
            return i;
        }
    }

    return (layout->count > 0) ? layout->count - 1 : 0;
}
