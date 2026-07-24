#include "renderer.h"
#include "app.h"
#include "config.h"
#include "crypto.h"
#include "theme.h"
#include "layout.h"

/* Shorthand for the active theme's colors */
#define THEME (theme_get(app.theme_index))

#include "bsp/display.h"
#include "pax_gfx.h"
#include "pax_text.h"
#include "pax_fonts.h"

/* Display names for the selectable editor fonts (all built into PAX,
 * so zero extra flash). The pax_font_* symbols are macros, so the
 * font itself is resolved in active_font() below. */
static const char *editor_font_names[EDITOR_FONT_COUNT] = {
    "Sky Mono",
    "Sky",
    "Saira",
    "Saira Cond",
    "Marker",
};

static const pax_font_t *active_font(void)
{
    switch (app.font_index)
    {
        case 1:  return pax_font_sky;
        case 2:  return pax_font_saira_regular;
        case 3:  return pax_font_saira_condensed;
        case 4:  return pax_font_marker;
        default: return pax_font_sky_mono;
    }
}

const char *renderer_font_name(int index)
{
    if (index < 0 || index >= EDITOR_FONT_COUNT)
    {
        index = 0;
    }
    return editor_font_names[index];
}



#include <string.h>
#include <stdio.h>

typedef struct
{
    pax_buf_t fb;

    /* Raw buffer dimensions (portrait 480×800) */
    size_t width;
    size_t height;

    /* Logical dimensions after PAX_O_ROT_CW (landscape 800×480) */
    int display_w;
    int display_h;

    bsp_display_color_format_t color;
    bsp_display_endianness_t endian;

    bool cursor_visible;

    /* Horizontal scroll offset for long lines (Nano-style) */
    int scroll_x;
    int scroll_line;

    /* Last known cursor screen position for fast blink updates */
    int cursor_screen_x;
    int cursor_screen_y;
    bool cursor_pos_valid;

} renderer_t;

static renderer_t renderer;

/* Shared line layout, rebuilt whenever the editor is drawn. Drawing,
 * cursor positioning and up/down navigation all read from this, so they
 * can never disagree about where a line ends. */
static Layout editor_layout;

/* Cache: skip layout rebuild when editor content hasn't changed */
static const char *layout_text_ptr  = NULL;
static size_t      layout_text_len  = 0;
static bool        layout_wrap      = false;
static int         layout_width     = 0;
static int         layout_font_size = 0;
static int         layout_font_idx  = 0;

static void draw_editor(const Editor *editor);

/* Width available for editor text, in pixels */
static int editor_text_width(void)
{
    return renderer.display_w - 10;
}

static int layout_measure(const char *text, size_t length)
{
    char tmp[LAYOUT_MEASURE_MAX];

    if (length >= sizeof(tmp))
    {
        length = sizeof(tmp) - 1;
    }

    memcpy(tmp, text, length);
    tmp[length] = '\0';

    return (int)pax_text_size(active_font(), app.font_size, tmp).x;
}

void renderer_rebuild_layout(const Editor *editor)
{
    int w = editor_text_width();

    if (editor->text == layout_text_ptr &&
        editor->length == layout_text_len &&
        app.word_wrap == layout_wrap &&
        w == layout_width &&
        app.font_size == layout_font_size &&
        app.font_index == layout_font_idx)
    {
        return; /* nothing changed */
    }

    layout_text_ptr  = editor->text;
    layout_text_len  = editor->length;
    layout_wrap      = app.word_wrap;
    layout_width     = w;
    layout_font_size = app.font_size;
    layout_font_idx  = app.font_index;

    layout_build(
        &editor_layout,
        editor,
        app.word_wrap,
        w,
        layout_measure);
}

const Layout *renderer_layout(void)
{
    return &editor_layout;
}

/*----------------------------------------------------------*/
/* Menu Setup */
/*----------------------------------------------------------*/
/* MENU_ITEM_COUNT is defined in app.h so app.c stays in sync */
static void menu_item_label(int index, char *buffer, size_t size)
{
    switch (index)
    {
        case 0: snprintf(buffer, size, "Save (Overwrite)"); break;
        case 1: snprintf(buffer, size, "Save As..."); break;
        case 2: snprintf(buffer, size, "Save Encrypted"); break;
        case 3: snprintf(buffer, size, "Load Text"); break;
        case 4: snprintf(buffer, size, "Load Encrypted"); break;
        case 5: snprintf(buffer, size, "Load Hex"); break;
        case 6: snprintf(buffer, size, "Text Size: %d", app.font_size); break;
        case 7: snprintf(buffer, size, "Font: %s",
                         renderer_font_name(app.font_index)); break;
        case 8: snprintf(buffer, size, "Word Wrap: %s",
                         app.word_wrap ? "ON" : "OFF"); break;
        case 9: snprintf(buffer, size, "Auto Capitals: %s",
                         app.auto_capitalize ? "ON" : "OFF"); break;
        case 10: snprintf(buffer, size, "Theme: %s",
                         theme_name(app.theme_index)); break;
        case 11: snprintf(buffer, size, "Markdown Preview"); break;
        case 12: snprintf(buffer, size, "Back to Editor"); break;
        case 13: snprintf(buffer, size, "Quit to Launcher"); break;
        default: buffer[0] = '\0'; break;
    }
}

/*----------------------------------------------------------*/

static void renderer_clear(void)
{
    pax_background(
        &renderer.fb,
        THEME->bg);

    renderer.cursor_pos_valid = false;
}

/*----------------------------------------------------------*/

static void renderer_present_internal(void)
{
    bsp_display_blit(
        0,
        0,
        renderer.width,
        renderer.height,
        pax_buf_get_pixels(&renderer.fb));
}

/*----------------------------------------------------------*/

static void draw_status_bar(const Editor *editor)
{
    pax_simple_rect(
        &renderer.fb,
        THEME->status_bg,
        0,
        renderer.display_h - STATUS_BAR_HEIGHT - HELP_BAR_HEIGHT,
        renderer.display_w,
        STATUS_BAR_HEIGHT);

    pax_draw_text(
        &renderer.fb,
        THEME->status_fg,
        active_font(),
        16,
        8,
        renderer.display_h - STATUS_BAR_HEIGHT - HELP_BAR_HEIGHT + 2,
        editor->status);
}

/*----------------------------------------------------------*/

/* Compute horizontal scroll to keep the cursor visible (Nano-style) */
static void renderer_compute_scroll(const Editor *editor)
{
    /* With word wrap on, nothing ever runs past the right edge. */
    if (app.word_wrap)
    {
        if (renderer.scroll_x != 0)
        {
            renderer.scroll_x = 0;
            renderer.cursor_pos_valid = false;
        }
        return;
    }

    int old_scroll = renderer.scroll_x;

    renderer_rebuild_layout(editor);
    const Layout *lay = renderer_layout();

    int line = layout_line_at(lay, editor->cursor);
    size_t start = lay->lines[line].start;
    size_t offset = editor->cursor - start;

    int cursor_pixel_x = 5 + layout_measure(editor->text + start, offset);

    /* Nano-style: keep cursor within a margin of the right edge */
    int margin = renderer.display_w / 4;
    int view_right = renderer.display_w - margin;
    int cursor_screen_x = cursor_pixel_x - renderer.scroll_x;

    if (cursor_screen_x > view_right)
    {
        renderer.scroll_x = cursor_pixel_x - view_right;
    }
    else if (cursor_screen_x < margin)
    {
        renderer.scroll_x = cursor_pixel_x - margin;
    }

    if (renderer.scroll_x < 0)
    {
        renderer.scroll_x = 0;
    }

    if (renderer.scroll_x != old_scroll)
    {
        renderer.cursor_pos_valid = false;
    }
}

/*----------------------------------------------------------*/

static void draw_editor(const Editor *editor)
{
    // Compute horizontal scroll to keep cursor visible
    renderer_compute_scroll(editor);

    int text_x = 5 - renderer.scroll_x;

    // 1. Draw static UI Header (Non-editable)
    pax_draw_text(
        &renderer.fb,
        THEME->accent,
        active_font(),
        16,
        5,
        5,
        "Tanmatsu Editor v" APP_VERSION " - System Ready");

    // 2. Start the editable text area below the header
    int font_size = app.font_size;
    int line_spacing = font_size + 2;

    int y = HEADER_HEIGHT + 2;

    renderer_rebuild_layout(editor);
    const Layout *lay = renderer_layout();

    int cursor_line = layout_line_at(lay, editor->cursor);

    /* Scroll vertically so the cursor line stays on screen */
    int usable_h = renderer.display_h - HEADER_HEIGHT - STATUS_BAR_HEIGHT
                   - HELP_BAR_HEIGHT - 2;
    int visible_lines = usable_h / line_spacing;
    if (visible_lines < 1) visible_lines = 1;

    if (cursor_line < renderer.scroll_line)
    {
        renderer.scroll_line = cursor_line;
    }
    else if (cursor_line >= renderer.scroll_line + visible_lines)
    {
        renderer.scroll_line = cursor_line - visible_lines + 1;
    }

    if (renderer.scroll_line > lay->count - 1)
    {
        renderer.scroll_line = (lay->count > 0) ? lay->count - 1 : 0;
    }
    if (renderer.scroll_line < 0)
    {
        renderer.scroll_line = 0;
    }

    char line[LAYOUT_MEASURE_MAX];
    int cursor_x = text_x;
    int cursor_y = y;
    bool cursor_found = false;

    for (int li = renderer.scroll_line; li < lay->count; li++)
    {
        if (li >= renderer.scroll_line + visible_lines)
        {
            break;
        }

        size_t start = lay->lines[li].start;
        size_t end   = lay->lines[li].end;
        size_t len   = end - start;

        if (len >= sizeof(line))
        {
            len = sizeof(line) - 1;
        }

        memcpy(line, editor->text + start, len);
        line[len] = '\0';

        /* Selection highlight behind the text */
        size_t sel_start, sel_end;
        if (editor_selection(editor, &sel_start, &sel_end) &&
            sel_start < end && sel_end > start)
        {
            size_t hi_from = (sel_start > start) ? sel_start - start : 0;
            size_t hi_to   = (sel_end < end) ? sel_end - start : len;

            int x1 = text_x + layout_measure(editor->text + start, hi_from);
            int x2 = text_x + layout_measure(editor->text + start, hi_to);

            pax_simple_rect(
                &renderer.fb,
                THEME->highlight,
                x1,
                y,
                x2 - x1,
                line_spacing);
        }

        pax_draw_text(
            &renderer.fb,
            THEME->fg,
            active_font(),
            font_size,
            text_x,
            y,
            line);

        /* Place the cursor if it lives on this line */
        if (li == cursor_line)
        {
            size_t offset = editor->cursor - start;
            if (offset > len) offset = len;

            char measured[LAYOUT_MEASURE_MAX];
            memcpy(measured, line, offset);
            measured[offset] = '\0';

            pax_vec1_t dims = pax_text_size(active_font(), font_size, measured);
            cursor_x = text_x + (int)dims.x;
            cursor_y = y;
            cursor_found = true;
        }

        y += line_spacing;
    }

    // Draw the cursor overlay
    if (cursor_found && renderer.cursor_visible)
    {
        pax_simple_rect(
            &renderer.fb,
            THEME->fg,
            cursor_x,
            cursor_y,
            CURSOR_WIDTH,
            font_size);

        renderer.cursor_screen_x = cursor_x;
        renderer.cursor_screen_y = cursor_y;
        renderer.cursor_pos_valid = true;
    }

    draw_status_bar(editor);
}

/*----------------------------------------------------------*/

static void renderer_update_cursor_only(const Editor *editor)
{
    renderer_compute_scroll(editor);

    int font_size = app.font_size;
    int line_spacing = font_size + 2;
    int text_x = 5 - renderer.scroll_x;

    renderer_rebuild_layout(editor);
    const Layout *lay = renderer_layout();

    int cursor_line = layout_line_at(lay, editor->cursor);

    /* If the cursor moved outside the visible window, the caller needs a
     * full redraw instead; draw_editor handles vertical scrolling. */
    int usable_h = renderer.display_h - HEADER_HEIGHT - STATUS_BAR_HEIGHT
                   - HELP_BAR_HEIGHT - 2;
    int visible_lines = usable_h / line_spacing;
    if (visible_lines < 1) visible_lines = 1;

    if (cursor_line < renderer.scroll_line ||
        cursor_line >= renderer.scroll_line + visible_lines)
    {
        draw_editor(editor);
        return;
    }

    int y = HEADER_HEIGHT + 2 + (cursor_line - renderer.scroll_line) * line_spacing;

    size_t start = lay->lines[cursor_line].start;
    size_t end   = lay->lines[cursor_line].end;
    size_t len   = end - start;

    char line_buf[LAYOUT_MEASURE_MAX];
    if (len >= sizeof(line_buf))
    {
        len = sizeof(line_buf) - 1;
    }
    memcpy(line_buf, editor->text + start, len);
    line_buf[len] = '\0';

    /* Clear just this line's row */
    pax_simple_rect(&renderer.fb, THEME->bg, 0, y, renderer.display_w, line_spacing);

    pax_draw_text(
        &renderer.fb,
        THEME->fg,
        active_font(),
        font_size,
        text_x,
        y,
        line_buf);

    /* Measure up to the cursor for its x position */
    size_t offset = editor->cursor - start;
    if (offset > len) offset = len;

    char measured[LAYOUT_MEASURE_MAX];
    memcpy(measured, line_buf, offset);
    measured[offset] = '\0';

    pax_vec1_t text_dims = pax_text_size(active_font(), font_size, measured);
    int cursor_x = text_x + (int)text_dims.x;

    if (renderer.cursor_visible)
    {
        pax_simple_rect(
            &renderer.fb,
            THEME->fg,
            cursor_x,
            y,
            CURSOR_WIDTH,
            font_size);

        renderer.cursor_screen_x = cursor_x;
        renderer.cursor_screen_y = y;
        renderer.cursor_pos_valid = true;
    }
    else
    {
        renderer.cursor_pos_valid = false;
    }
}

/*----------------------------------------------------------*/

static void renderer_draw_menu(int menu_selected)
{
    pax_draw_text(
        &renderer.fb,
        THEME->fg,
        active_font(),
        16,
        5,
        5,
        "--- MAIN MENU ---");

    for (int i = 0; i < MENU_ITEM_COUNT; i++)
    {
        int y = 30 + i * MENU_ITEM_HEIGHT;

        if (i == menu_selected)
        {
            pax_simple_rect(
                &renderer.fb,
                THEME->highlight,
                5,
                y,
                220,
                MENU_ITEM_HEIGHT - 2);
        }

        char label[48];
        menu_item_label(i, label, sizeof(label));

        pax_draw_text(
            &renderer.fb,
            THEME->fg,
            active_font(),
            16,
            10,
            y + 2,
            label);
    }
}

/*----------------------------------------------------------*/

static void renderer_update_menu(int prev_selected, int new_selected)
{
    // Only redraw the two affected rows instead of the full menu

    char label[48];

    // Clear and redraw the old selection row (remove highlight)
    int old_y = 30 + prev_selected * MENU_ITEM_HEIGHT;
    pax_simple_rect(&renderer.fb, THEME->bg, 0, old_y, renderer.display_w, MENU_ITEM_HEIGHT);
    menu_item_label(prev_selected, label, sizeof(label));
    pax_draw_text(
        &renderer.fb,
        THEME->fg,
        active_font(),
        16,
        10,
        old_y + 2,
        label);

    // Clear and redraw the new selection row (add highlight)
    int new_y = 30 + new_selected * MENU_ITEM_HEIGHT;
    pax_simple_rect(&renderer.fb, THEME->bg, 0, new_y, renderer.display_w, MENU_ITEM_HEIGHT);
    pax_simple_rect(
        &renderer.fb,
        THEME->highlight,
        5,
        new_y,
        220,
        MENU_ITEM_HEIGHT - 2);
    menu_item_label(new_selected, label, sizeof(label));
    pax_draw_text(
        &renderer.fb,
        THEME->fg,
        active_font(),
        16,
        10,
        new_y + 2,
        label);
}

/*----------------------------------------------------------*/

static void draw_browser(const Browser *browser, const Filesystem *fs)
{
    pax_simple_rect(
        &renderer.fb,
        THEME->header_bg,
        0,
        0,
        renderer.display_w,
        HEADER_HEIGHT);

    pax_draw_text(
        &renderer.fb,
        THEME->fg,
        active_font(),
        16,
        5,
        5,
        "File Browser");

    pax_draw_text(
        &renderer.fb,
        THEME->dim,
        active_font(),
        16,
        5,
        22,
        fs->current_path);

    int visible =
        (renderer.display_h - HEADER_HEIGHT - HELP_BAR_HEIGHT) /
        MENU_ITEM_HEIGHT;

    /* Update the browser's visible item count for scroll tracking */
    ((Browser *)browser)->visible_items = visible;

    for (int i = 0; i < visible; i++)
    {
        int idx = i + browser->scroll;

        if (idx >= browser->count)
            break;

        int y = HEADER_HEIGHT + i * MENU_ITEM_HEIGHT;

        pax_col_t bg =
            (i & 1)
                ? THEME->row2
                : THEME->row1;

        if (idx == browser->selected)
            bg = THEME->highlight;

        pax_simple_rect(
            &renderer.fb,
            bg,
            0,
            y,
            renderer.display_w - 10,
            MENU_ITEM_HEIGHT);

        char text[96];

        snprintf(
            text,
            sizeof(text),
            "%s %s",
            browser->entries[idx].is_directory
                ? "[DIR ]"
                : "[FILE]",
            browser->entries[idx].name);

        pax_draw_text(
            &renderer.fb,
            THEME->fg,
            active_font(),
            16,
            10,
            y + 2,
            text);
    }
}

/*----------------------------------------------------------*/

static void renderer_update_browser(
    const Browser *browser,
    const Filesystem *fs,
    int prev_selected,
    int prev_scroll)
{
    // If scroll changed, the entire list shifted — fall back to full redraw
    if (browser->scroll != prev_scroll)
    {
        draw_browser(browser, fs);
        return;
    }

    int visible =
        (renderer.display_h - HEADER_HEIGHT - HELP_BAR_HEIGHT) /
        MENU_ITEM_HEIGHT;

    // Clear and redraw the old selection row
    int old_row = prev_selected - browser->scroll;
    if (old_row >= 0 && old_row < visible)
    {
        int y = HEADER_HEIGHT + old_row * MENU_ITEM_HEIGHT;
        pax_col_t bg = (old_row & 1) ? THEME->row2 : THEME->row1;
        pax_simple_rect(&renderer.fb, bg, 0, y, renderer.display_w - 10, MENU_ITEM_HEIGHT);

        int idx = old_row + browser->scroll;
        if (idx < browser->count)
        {
            char text[96];
            snprintf(text, sizeof(text), "%s %s",
                browser->entries[idx].is_directory ? "[DIR ]" : "[FILE]",
                browser->entries[idx].name);
            pax_draw_text(&renderer.fb, THEME->fg, active_font(), 16, 10, y + 2, text);
        }
    }

    // Clear and redraw the new selection row
    int new_row = browser->selected - browser->scroll;
    if (new_row >= 0 && new_row < visible)
    {
        int y = HEADER_HEIGHT + new_row * MENU_ITEM_HEIGHT;
        pax_simple_rect(&renderer.fb, THEME->highlight, 0, y, renderer.display_w - 10, MENU_ITEM_HEIGHT);

        int idx = new_row + browser->scroll;
        if (idx < browser->count)
        {
            char text[96];
            snprintf(text, sizeof(text), "%s %s",
                browser->entries[idx].is_directory ? "[DIR ]" : "[FILE]",
                browser->entries[idx].name);
            pax_draw_text(&renderer.fb, THEME->fg, active_font(), 16, 10, y + 2, text);
        }
    }
}

/*----------------------------------------------------------*/

static void draw_save_as(const AppContext *context)
{
    // Draw the editor in the background for a layered effect
    draw_editor(&context->editor);

    // Calculate popup dimensions
    int box_w = renderer.display_w - 16;
    int box_h = 70;
    int box_x = (renderer.display_w - box_w) / 2;
    int box_y = (renderer.display_h - box_h) / 2;
    pax_simple_rect(&renderer.fb, THEME->accent, box_x - 2, box_y - 2, box_w + 4, box_h + 4);
    pax_simple_rect(&renderer.fb, THEME->bg, box_x, box_y, box_w, box_h);

    // Draw the title
    pax_draw_text(
        &renderer.fb, THEME->fg, active_font(), 16, box_x + 10, box_y + 10, 
        "Enter new filename:");

    // Draw the typed text with a blinking underscore cursor
    char input_display[68];
    snprintf(
        input_display, sizeof(input_display), "%s%s", 
        context->prompt_buffer, 
        renderer.cursor_visible ? "_" : " ");
        
    pax_draw_text(
        &renderer.fb, THEME->fg, active_font(), 16, box_x + 10, box_y + 35, 
        input_display);
}

/*----------------------------------------------------------*/

static void draw_crypto_menu(const AppContext *context)
{
    draw_editor(&context->editor);

    // Dynamic height based on how many ciphers exist in the enum
    int box_w = renderer.display_w - 32;
    int box_h = 40 + (CRYPTO_METHOD_COUNT * MENU_ITEM_HEIGHT);
    int box_x = (renderer.display_w - box_w) / 2;
    int box_y = (renderer.display_h - box_h) / 2;

    // Purple border for crypto selection
    pax_simple_rect(&renderer.fb, 0xFF800080, box_x - 2, box_y - 2, box_w + 4, box_h + 4);
    pax_simple_rect(&renderer.fb, THEME->bg, box_x, box_y, box_w, box_h);

    pax_draw_text(
        &renderer.fb, THEME->fg, active_font(), 16, box_x + 10, box_y + 10, 
        "Select Cipher Method:");

    for (int i = 0; i < CRYPTO_METHOD_COUNT; i++)
    {
        int item_y = box_y + 35 + (i * MENU_ITEM_HEIGHT);

        if (i == context->selected_cipher)
        {
            pax_simple_rect(&renderer.fb, THEME->highlight, box_x + 5, item_y, box_w - 10, MENU_ITEM_HEIGHT - 2);
        }

        // Fetch the name dynamically from our crypto module
        pax_draw_text(
            &renderer.fb, THEME->fg, active_font(), 16, box_x + 10, item_y + 2, 
            crypto_method_name((CryptoMethod)i));
    }
}

/*----------------------------------------------------------*/

static void draw_password(const AppContext *context)
{
    // Layer over the editor
    draw_editor(&context->editor);

    int box_w = renderer.display_w - 16;
    int box_h = 70;
    int box_x = (renderer.display_w - box_w) / 2;
    int box_y = (renderer.display_h - box_h) / 2;

    // Red border indicates security action
    pax_simple_rect(&renderer.fb, COLOR_RED, box_x - 2, box_y - 2, box_w + 4, box_h + 4);
    pax_simple_rect(&renderer.fb, THEME->bg, box_x, box_y, box_w, box_h);

    const char *title = context->is_saving_encrypted ? "Set Password to Encrypt:" : "Enter Password to Decrypt:";
    pax_draw_text(
        &renderer.fb, THEME->fg, active_font(), 16, box_x + 10, box_y + 10, 
        title);

    // Generate masked string
    char masked_display[36];
    size_t i = 0;
    for (; i < context->password_cursor; i++) 
    {
        masked_display[i] = '*';
    }
    masked_display[i] = '\0';
    
    // Append the blinking cursor
    snprintf(
        masked_display + i, sizeof(masked_display) - i, "%s", 
        renderer.cursor_visible ? "_" : " ");
        
    pax_draw_text(
        &renderer.fb, THEME->fg, active_font(), 16, box_x + 10, box_y + 35, 
        masked_display);
}

/*----------------------------------------------------------*/

/*----------------------------------------------------------*/
/* Markdown Preview */
/*----------------------------------------------------------*/

/* Rewrites markdown links and images to readable text:
 *   [text](url)  -> text
 *   ![alt](url)  -> [img: alt]
 * Anything that isn't a complete link stays untouched. */
static size_t md_strip_links(const char *src, size_t len, char *dst, size_t cap)
{
    size_t o = 0;
    size_t i = 0;

    while (i < len && o < cap - 1)
    {
        bool image = (src[i] == '!' && i + 1 < len && src[i + 1] == '[');

        if (src[i] == '[' || image)
        {
            size_t lb = i + (image ? 1 : 0);
            size_t rb = lb + 1;
            while (rb < len && src[rb] != ']') rb++;

            if (rb < len && rb + 1 < len && src[rb + 1] == '(')
            {
                size_t rp = rb + 2;
                while (rp < len && src[rp] != ')') rp++;

                if (rp < len)
                {
                    if (image)
                    {
                        const char *tag = "[img: ";
                        for (const char *t = tag; *t && o < cap - 1; t++) dst[o++] = *t;
                    }
                    for (size_t k = lb + 1; k < rb && o < cap - 1; k++) dst[o++] = src[k];
                    if (image && o < cap - 1) dst[o++] = ']';
                    i = rp + 1;
                    continue;
                }
            }
        }

        dst[o++] = src[i++];
    }

    dst[o] = '\0';
    return o;
}

/* Draws one word at (*x, *y), wrapping to indent_x when it doesn't fit.
 * code words get a highlight background. */
static void md_word(
    const char *word, size_t len,
    int *x, int *y, int indent_x, int wrap_w,
    int size, uint32_t color, bool code)
{
    char buf[64];
    if (len >= sizeof(buf)) len = sizeof(buf) - 1;
    memcpy(buf, word, len);
    buf[len] = '\0';

    int w = (int)pax_text_size(active_font(), size, buf).x;

    if (*x + w > wrap_w && *x > indent_x)
    {
        *x = indent_x;
        *y += size + 4;
    }

    int bottom = renderer.display_h - HELP_BAR_HEIGHT;

    if (*y + size >= 0 && *y <= bottom)
    {
        if (code)
        {
            pax_simple_rect(&renderer.fb, THEME->highlight,
                            *x - 1, *y, w + 2, size + 2);
        }

        pax_draw_text(&renderer.fb, color, active_font(), size, *x, *y, buf);
    }

    *x += w + size / 3;
}

/* Renders one logical markdown line, applying and stripping the inline
 * markers for bold (double asterisk or double underscore), italic
 * (single asterisk or underscore) and inline code (backticks).
 * Returns the new y. */
static int md_flow(
    const char *text, size_t len,
    int indent_x, int y, int size, uint32_t base_color)
{
    int wrap_w = renderer.display_w - 8;
    int x = indent_x;

    bool bold = false, italic = false, code = false;

    size_t i = 0;
    while (i < len)
    {
        while (i < len && text[i] == ' ') i++;
        if (i >= len) break;

        size_t ws = i;
        while (i < len && text[i] != ' ') i++;
        size_t we = i;

        /* Leading markers toggle style on and are stripped */
        while (ws < we)
        {
            if (we - ws >= 2 &&
                ((text[ws] == '*' && text[ws + 1] == '*') ||
                 (text[ws] == '_' && text[ws + 1] == '_')))
            {
                bold = !bold; ws += 2;
            }
            else if (text[ws] == '*' || text[ws] == '_')
            {
                italic = !italic; ws += 1;
            }
            else if (text[ws] == '`') { code = !code; ws += 1; }
            else break;
        }

        /* Trailing markers are stripped now, toggle style after drawing */
        int post_bold = 0, post_italic = 0, post_code = 0;
        while (we > ws)
        {
            if (we - ws >= 2 &&
                ((text[we - 2] == '*' && text[we - 1] == '*') ||
                 (text[we - 2] == '_' && text[we - 1] == '_')))
            {
                post_bold++; we -= 2;
            }
            else if (text[we - 1] == '*' || text[we - 1] == '_')
            {
                post_italic++; we -= 1;
            }
            else if (text[we - 1] == '`') { post_code++; we -= 1; }
            else break;
        }

        uint32_t color = base_color;
        if (bold)   color = THEME->accent;
        if (italic) color = THEME->dim;

        if (we > ws)
        {
            md_word(text + ws, we - ws, &x, &y, indent_x, wrap_w,
                    size, color, code);
        }

        if (post_bold   % 2) bold   = !bold;
        if (post_italic % 2) italic = !italic;
        if (post_code   % 2) code   = !code;
    }

    return y + size + 4;
}

static void draw_md_preview(const Editor *editor)
{
    static char raw[EDITOR_BUFFER_SIZE];
    static char line[EDITOR_BUFFER_SIZE];

    int base = app.font_size;
    int y = 8 - app.md_scroll;
    size_t col = 0;
    bool in_code = false;
    int bottom = renderer.display_h - HELP_BAR_HEIGHT;

    for (size_t i = 0; i <= editor->length; i++)
    {
        char c = (i < editor->length) ? editor->text[i] : '\0';

        if (c != '\n' && c != '\0')
        {
            if (col < sizeof(raw) - 1) raw[col++] = c;
            continue;
        }

        raw[col] = '\0';
        size_t len = col;
        col = 0;

        /* Fenced code blocks: ``` toggles, language tag line is skipped */
        if (len >= 3 && raw[0] == '`' && raw[1] == '`' && raw[2] == '`')
        {
            in_code = !in_code;
            y += 4;
            goto next;
        }

        if (in_code)
        {
            if (y + base >= 0 && y <= bottom)
            {
                pax_simple_rect(&renderer.fb, THEME->highlight,
                                4, y - 1, renderer.display_w - 8, base + 4);
                pax_draw_text(&renderer.fb, THEME->fg, pax_font_sky_mono,
                              base, 8, y, raw);
            }
            y += base + 4;
            goto next;
        }

        if (len == 0)
        {
            y += base / 2 + 4;
            goto next;
        }

        /* Leading spaces control nesting depth (4 spaces per level) */
        size_t sp = 0;
        while (sp < len && raw[sp] == ' ') sp++;
        int indent_extra = (int)(sp / 4) * 16;
        char *ln = raw + sp;
        size_t ll = len - sp;

        if (ll == 0)
        {
            y += base / 2 + 4;
            goto next;
        }

        /* Blockquotes: > and >> get vertical bars and dimmed text */
        if (ln[0] == '>')
        {
            int depth = 0;
            size_t q = 0;
            while (q < ll && (ln[q] == '>' || ln[q] == ' '))
            {
                if (ln[q] == '>') depth++;
                q++;
            }
            if (depth > 4) depth = 4;

            int text_y = y;
            size_t sl = md_strip_links(ln + q, ll - q, line, sizeof(line));
            y = md_flow(line, sl, 5 + depth * 8 + 8, y, base, THEME->dim);

            if (text_y + base >= 0 && text_y <= bottom)
            {
                for (int b = 0; b < depth; b++)
                {
                    pax_simple_rect(&renderer.fb, THEME->dim,
                                    5 + b * 8, text_y, 3, y - text_y - 2);
                }
            }
            goto next;
        }

        /* Headers: # through ###### */
        if (ln[0] == '#')
        {
            int level = 0;
            while ((size_t)level < ll && ln[level] == '#' && level < 6) level++;
            size_t skip = level;
            while (skip < ll && ln[skip] == ' ') skip++;

            static const int add[6] = { 10, 6, 4, 3, 2, 1 };
            size_t sl = md_strip_links(ln + skip, ll - skip, line, sizeof(line));
            y = md_flow(line, sl, 5, y, base + add[level - 1], THEME->accent);
            y += 2;
            goto next;
        }

        /* Tables: verbatim in the mono font so columns stay aligned */
        if (ln[0] == '|')
        {
            if (y + base >= 0 && y <= bottom)
            {
                pax_draw_text(&renderer.fb, THEME->fg, pax_font_sky_mono,
                              base, 5 + indent_extra, y, ln);
            }
            y += base + 4;
            goto next;
        }

        /* Unordered list items: -, * or + followed by a space */
        if (ll >= 2 && (ln[0] == '-' || ln[0] == '*' || ln[0] == '+') &&
            ln[1] == ' ')
        {
            int bx = 8 + indent_extra;
            if (y + base >= 0 && y <= bottom)
            {
                pax_simple_rect(&renderer.fb, THEME->fg,
                                bx, y + base / 2 - 1, 4, 4);
            }
            size_t sl = md_strip_links(ln + 2, ll - 2, line, sizeof(line));
            y = md_flow(line, sl, bx + 12, y, base, THEME->fg);
            goto next;
        }

        /* Ordered list items: digits + ". " — the number itself stays */
        {
            size_t d = 0;
            while (d < ll && ln[d] >= '0' && ln[d] <= '9') d++;

            if (d > 0 && d + 1 < ll && ln[d] == '.' && ln[d + 1] == ' ')
            {
                size_t sl = md_strip_links(ln, ll, line, sizeof(line));
                y = md_flow(line, sl, 8 + indent_extra, y, base, THEME->fg);
                goto next;
            }
        }

        /* Plain paragraph */
        {
            size_t sl = md_strip_links(ln, ll, line, sizeof(line));
            y = md_flow(line, sl, 5 + indent_extra, y, base, THEME->fg);
        }

next:
        if (i >= editor->length) break;
    }
}

static void draw_help_bar(const AppContext *context)
{
    int bar_y = renderer.display_h - HELP_BAR_HEIGHT;

    pax_simple_rect(
        &renderer.fb,
        THEME->header_bg,
        0,
        bar_y,
        renderer.display_w,
        HELP_BAR_HEIGHT);

    const char *text = "";

    switch (context->state)
    {
        case APP_STATE_EDITOR:
            text = "ESC:Menu Shift+Arr:Select Ctrl:A/C/X/V/M Su+Shift:Caps";
            break;
        case APP_STATE_MENU:
            text = "Up/Dn:Nav Enter:Sel L/R:Adjust ESC:Back";
            break;

        case APP_STATE_MD_PREVIEW:
            text = "Up/Dn:Scroll ESC/Ctrl+M:Editor";
            break;
        case APP_STATE_BROWSER:
            text = "Up/Dn:Nav Enter:Open ESC:Back";
            break;
        case APP_STATE_CRYPTO_SELECT:
            text = "Up/Dn:Nav Enter:Sel ESC:Back";
            break;
        case APP_STATE_SAVE_AS:
            text = "Enter:Save ESC:Cancel";
            break;
        case APP_STATE_PASSWORD:
            text = "Enter:OK ESC:Cancel";
            break;
    }

    pax_draw_text(
        &renderer.fb,
        THEME->fg,
        active_font(),
        16,
        8,
        bar_y + 1,
        text);
}

/*----------------------------------------------------------*/

bool renderer_init(void)
{
    bsp_display_get_parameters(
        &renderer.width,
        &renderer.height,
        &renderer.color,
        &renderer.endian);

    pax_buf_type_t type =
        PAX_BUF_24_888RGB;

    switch (renderer.color)
    {
        case BSP_DISPLAY_COLOR_FORMAT_16_565RGB:
            type = PAX_BUF_16_565RGB;
            break;

        case BSP_DISPLAY_COLOR_FORMAT_32_8888ARGB:
            type = PAX_BUF_32_8888ARGB;
            break;

        default:
            break;
    }

    if (!pax_buf_init(
            &renderer.fb,
            NULL,
            renderer.width,
            renderer.height,
            type))
    {
        return false;
    }

    pax_buf_reversed(
        &renderer.fb,
        renderer.endian ==
        BSP_DISPLAY_ENDIAN_BIG);

    pax_buf_set_orientation(
        &renderer.fb,
        PAX_O_ROT_CW);

    renderer.display_w = pax_buf_get_width(&renderer.fb);
    renderer.display_h = pax_buf_get_height(&renderer.fb);

    renderer.cursor_visible = true;
    renderer.cursor_pos_valid = false;

    return true;
}

/*----------------------------------------------------------*/

void renderer_cursor_blink(void)
{
    renderer.cursor_visible =
        !renderer.cursor_visible;
}

/*----------------------------------------------------------*/

/* Fast cursor blink: only erases/redraws the cursor rectangle
 * at the last known position. No layout rebuild, no text redraw. */
static void renderer_blink_cursor(void)
{
    if (!renderer.cursor_pos_valid) return;

    int x = renderer.cursor_screen_x;
    int y = renderer.cursor_screen_y;
    int h = app.font_size;

    /* Erase the old cursor */
    pax_simple_rect(&renderer.fb, THEME->bg, x, y, CURSOR_WIDTH, h);

    /* Draw the new cursor if visible */
    if (renderer.cursor_visible)
    {
        pax_simple_rect(&renderer.fb, THEME->fg, x, y, CURSOR_WIDTH, h);
    }
}

/*----------------------------------------------------------*/

void renderer_render(AppContext *context)
{
    if (context->redraw_request == REDRAW_NONE) 
    {
        return;
    }

    // --- FAST PATH: Cursor Blink Only (no text change) ---
    if (context->redraw_request == REDRAW_CURSOR && context->state == APP_STATE_EDITOR)
    {
        renderer_blink_cursor();
        renderer_present_internal();
        
        context->redraw_request = REDRAW_NONE;
        return;
    }

    // --- FAST PATH: Line Changed ---
    if (context->redraw_request == REDRAW_LINE && context->state == APP_STATE_EDITOR)
    {
        renderer_update_cursor_only(&context->editor);
        renderer_present_internal();
        
        context->redraw_request = REDRAW_NONE;
        return;
    }

    // --- FAST PATH: Menu Selection Changed ---
    if (context->redraw_request == REDRAW_MENU && context->state == APP_STATE_MENU)
    {
        renderer_update_menu(context->prev_menu_selected, context->menu_selected);
        renderer_present_internal();
        
        context->redraw_request = REDRAW_NONE;
        return;
    }

    // --- FAST PATH: Browser Selection Changed ---
    if (context->redraw_request == REDRAW_BROWSER && context->state == APP_STATE_BROWSER)
    {
        renderer_update_browser(&context->browser, &context->filesystem, context->prev_browser_selected, context->prev_browser_scroll);
        renderer_present_internal();
        
        context->redraw_request = REDRAW_NONE;
        return;
    }

    // --- FULL PATH: Standard Redraw ---
    renderer_clear();

    switch (context->state)
    {
        case APP_STATE_EDITOR:
            draw_editor(&context->editor);
            break;

        case APP_STATE_MENU:
            renderer_draw_menu(context->menu_selected);
            break;

        case APP_STATE_BROWSER:
            draw_browser(&context->browser, &context->filesystem);
            break;
            
        case APP_STATE_SAVE_AS:
            draw_save_as(context);
            break;

        case APP_STATE_CRYPTO_SELECT:
            draw_crypto_menu(context);
            break;
            
        case APP_STATE_PASSWORD:
            draw_password(context);
            break;

        case APP_STATE_MD_PREVIEW:
            draw_md_preview(&context->editor);
            break;
    }

    draw_help_bar(context);

    renderer_present_internal();
    
    // Clear the flag when done
    context->redraw_request = REDRAW_NONE; 
}
