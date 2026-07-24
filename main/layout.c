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
                size_t total = hard_end - line_start;

                /* Binary search: find the maximum number of chars that fit */
                size_t lo = 0, hi = total;
                while (lo < hi)
                {
                    size_t mid = lo + (hi - lo + 1) / 2;
                    if (measure(editor->text + line_start, mid) <= max_width)
                        lo = mid;
                    else
                        hi = mid - 1;
                }

                size_t fit = lo;

                if (fit >= total)
                {
                    /* The remainder fits on one line */
                    layout_push(layout, line_start, hard_end,
                                hard_end < editor->length);
                    line_start = hard_end + 1;
                    break;
                }

                /* Find the last space in the fitting portion for word break */
                size_t last_space = 0;
                for (size_t j = line_start; j < line_start + fit; j++)
                {
                    if (editor->text[j] == ' ')
                    {
                        last_space = j;
                    }
                }

                /* Prefer breaking after the last space so we don't split a
                 * word. Fall back to a hard character break for a single
                 * word longer than the screen. */
                size_t break_at;

                if (last_space > line_start)
                {
                    break_at = last_space + 1;
                }
                else
                {
                    break_at = (fit > 0) ? line_start + fit : line_start + 1;
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
    /* Binary search: lines are sorted by start position */
    int lo = 0, hi = layout->count - 1;

    while (lo < hi)
    {
        int mid = lo + (hi - lo + 1) / 2;
        if (layout->lines[mid].start <= position)
            lo = mid;
        else
            hi = mid - 1;
    }

    if (lo >= 0 && lo < layout->count)
    {
        int i = lo;
        /* A position sitting exactly on a soft break belongs to the
         * next line, so the cursor appears where typing continues. */
        if (position == layout->lines[i].end &&
            !layout->lines[i].hard_break &&
            i + 1 < layout->count &&
            layout->lines[i + 1].start == position)
        {
            return i + 1;
        }
        return i;
    }

    return (layout->count > 0) ? layout->count - 1 : 0;
}
