#include "renderer.h"
#include "app.h"
#include "config.h"
#include "crypto.h"

#include "bsp/display.h"
#include "pax_gfx.h"
#include "pax_text.h"
#include "pax_fonts.h"

#include <string.h>
#include <stdio.h>

typedef struct
{
    pax_buf_t fb;

    size_t width;
    size_t height;

    bsp_display_color_format_t color;
    bsp_display_endianness_t endian;

    bool cursor_visible;

} renderer_t;

static renderer_t renderer;

/*----------------------------------------------------------*/
/* Menu Setup */
/*----------------------------------------------------------*/
static const char* menu_items[] = {
    "Save (Overwrite)", 
    "Save As...", 
    "Save Encrypted", 
    "Load Text", 
    "Load Encrypted", 
    "Load Hex", 
    "Back to Editor"
};
static const int MENU_ITEM_COUNT = 7;

/*----------------------------------------------------------*/

static void renderer_clear(void)
{
    pax_background(
        &renderer.fb,
        COLOR_BLACK);
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
        COLOR_RED,
        0,
        renderer.height - STATUS_BAR_HEIGHT,
        renderer.width,
        STATUS_BAR_HEIGHT);

    pax_draw_text(
        &renderer.fb,
        COLOR_WHITE,
        pax_font_sky_mono,
        16,
        8,
        renderer.height - STATUS_BAR_HEIGHT + 2,
        editor->status);
}

/*----------------------------------------------------------*/

static void draw_editor(const Editor *editor)
{
    // 1. Draw static UI Header (Non-editable)
    pax_draw_text(
        &renderer.fb,
        COLOR_BLUE,
        pax_font_sky_mono,
        16,
        5,
        5,
        "Tanmatsu Editor v1.2.1 - System Ready");

    // 2. Start the editable text area below the header
    int y = 30; 
    int col = 0;
    char line[128];

    // Track cursor position
    int cursor_x = 5;
    int cursor_y = y;
    bool cursor_found = false;

    for (size_t i = 0; i <= editor->length; i++)
    {
        if (i == editor->cursor)
        {
            // Temporarily null-terminate the line to measure its exact pixel width
            line[col] = '\0';
            pax_vec1_t text_dims = pax_text_size(pax_font_sky_mono, 16, line);
            
            cursor_x = 5 + (int)text_dims.x;
            cursor_y = y;
            cursor_found = true;
        }

        char c = editor->text[i];

        if (c == '\n' || c == '\0' || col >= sizeof(line) - 1)
        {
            line[col] = '\0';

            pax_draw_text(
                &renderer.fb,
                COLOR_WHITE,
                pax_font_sky_mono,
                16,
                5,
                y,
                line);

            y += LINE_SPACING;
            col = 0;

            // Stop if we hit the bottom of the screen (above the status bar)
            if (y > renderer.height - STATUS_BAR_HEIGHT - LINE_SPACING)
            {
                break;
            }
        }
        else
        {
            line[col++] = c;
        }
    }

    // Draw the cursor overlay
    if (cursor_found && renderer.cursor_visible)
    {
        pax_simple_rect(
            &renderer.fb,
            COLOR_WHITE,
            cursor_x,
            cursor_y,
            CURSOR_WIDTH,
            FONT_HEIGHT);
    }

    draw_status_bar(editor);
}

/*----------------------------------------------------------*/

static void renderer_update_cursor_only(const Editor *editor)
{
    int y = 30; // Matches the editable area start height
    int col = 0;
    char line_buf[128];
    
    // Build the current line up to the cursor
    for (size_t i = 0; i < editor->cursor; i++) 
    {
        char c = editor->text[i];
        
        if (c == '\n' || col >= sizeof(line_buf) - 1) 
        {
            y += LINE_SPACING;
            col = 0;
        } 
        else 
        {
            line_buf[col++] = c;
        }
    }

    // Null-terminate and measure the string
    line_buf[col] = '\0';
    pax_vec1_t text_dims = pax_text_size(pax_font_sky_mono, 16, line_buf);

    int cursor_x = 5 + (int)text_dims.x;
    int cursor_y = y;

    // Draw EITHER a white box (visible) OR a black box (hidden)
    pax_col_t cursor_color = renderer.cursor_visible ? COLOR_WHITE : COLOR_BLACK;
    
    // Paint just the cursor rectangle directly onto the existing framebuffer
    pax_simple_rect(
        &renderer.fb, 
        cursor_color, 
        cursor_x, 
        cursor_y, 
        CURSOR_WIDTH, 
        FONT_HEIGHT);
}

/*----------------------------------------------------------*/

void renderer_draw_menu(int menu_selected)
{
    pax_draw_text(
        &renderer.fb,
        COLOR_WHITE,
        pax_font_sky_mono,
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
                COLOR_BLUE,
                5,
                y,
                220,
                MENU_ITEM_HEIGHT - 2);
        }

        pax_draw_text(
            &renderer.fb,
            COLOR_WHITE,
            pax_font_sky_mono,
            16,
            10,
            y + 2,
            menu_items[i]);
    }
}

/*----------------------------------------------------------*/

static void draw_browser(const Browser *browser, const Filesystem *fs)
{
    pax_simple_rect(
        &renderer.fb,
        COLOR_BG_HEADER,
        0,
        0,
        renderer.width,
        HEADER_HEIGHT);

    pax_draw_text(
        &renderer.fb,
        COLOR_WHITE,
        pax_font_sky_mono,
        16,
        5,
        5,
        "File Browser");

    pax_draw_text(
        &renderer.fb,
        COLOR_SCROLL_FG,
        pax_font_sky_mono,
        16,
        5,
        22,
        fs->current_path);

    int visible =
        (renderer.height - HEADER_HEIGHT) /
        MENU_ITEM_HEIGHT;

    for (int i = 0; i < visible; i++)
    {
        int idx = i + browser->scroll;

        if (idx >= browser->count)
            break;

        int y = HEADER_HEIGHT + i * MENU_ITEM_HEIGHT;

        pax_col_t bg =
            (i & 1)
                ? COLOR_BG_ROW2
                : COLOR_BG_ROW1;

        if (idx == browser->selected)
            bg = COLOR_BLUE;

        pax_simple_rect(
            &renderer.fb,
            bg,
            0,
            y,
            renderer.width - 10,
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
            COLOR_WHITE,
            pax_font_sky_mono,
            16,
            10,
            y + 2,
            text);
    }
}

/*----------------------------------------------------------*/

static void draw_save_as(const AppContext *context)
{
    // Draw the editor in the background for a layered effect
    draw_editor(&context->editor);

    // Calculate popup dimensions
    int box_w = 280;
    int box_h = 70;
    int box_x = (renderer.width - box_w) / 2;
    int box_y = (renderer.height - box_h) / 2;

    // Draw an outer border (Blue) and inner background (Black)
    pax_simple_rect(&renderer.fb, COLOR_BLUE, box_x - 2, box_y - 2, box_w + 4, box_h + 4);
    pax_simple_rect(&renderer.fb, COLOR_BLACK, box_x, box_y, box_w, box_h);

    // Draw the title
    pax_draw_text(
        &renderer.fb, COLOR_WHITE, pax_font_sky_mono, 16, box_x + 10, box_y + 10, 
        "Enter new filename:");

    // Draw the typed text with a blinking underscore cursor
    char input_display[68];
    snprintf(
        input_display, sizeof(input_display), "%s%s", 
        context->prompt_buffer, 
        renderer.cursor_visible ? "_" : " ");
        
    pax_draw_text(
        &renderer.fb, COLOR_WHITE, pax_font_sky_mono, 16, box_x + 10, box_y + 35, 
        input_display);
}

/*----------------------------------------------------------*/

static void draw_crypto_menu(const AppContext *context)
{
    draw_editor(&context->editor);

    // Dynamic height based on how many ciphers exist in the enum
    int box_w = 260;
    int box_h = 40 + (CRYPTO_METHOD_COUNT * MENU_ITEM_HEIGHT);
    int box_x = (renderer.width - box_w) / 2;
    int box_y = (renderer.height - box_h) / 2;

    // Purple border for crypto selection
    pax_simple_rect(&renderer.fb, 0xFF800080, box_x - 2, box_y - 2, box_w + 4, box_h + 4);
    pax_simple_rect(&renderer.fb, COLOR_BLACK, box_x, box_y, box_w, box_h);

    pax_draw_text(
        &renderer.fb, COLOR_WHITE, pax_font_sky_mono, 16, box_x + 10, box_y + 10, 
        "Select Cipher Method:");

    for (int i = 0; i < CRYPTO_METHOD_COUNT; i++)
    {
        int item_y = box_y + 35 + (i * MENU_ITEM_HEIGHT);

        if (i == context->selected_cipher)
        {
            pax_simple_rect(&renderer.fb, COLOR_BLUE, box_x + 5, item_y, box_w - 10, MENU_ITEM_HEIGHT - 2);
        }

        // Fetch the name dynamically from our crypto module
        pax_draw_text(
            &renderer.fb, COLOR_WHITE, pax_font_sky_mono, 16, box_x + 10, item_y + 2, 
            crypto_method_name((CryptoMethod)i));
    }
}

/*----------------------------------------------------------*/

static void draw_password(const AppContext *context)
{
    // Layer over the editor
    draw_editor(&context->editor);

    int box_w = 280;
    int box_h = 70;
    int box_x = (renderer.width - box_w) / 2;
    int box_y = (renderer.height - box_h) / 2;

    // Red border indicates security action
    pax_simple_rect(&renderer.fb, COLOR_RED, box_x - 2, box_y - 2, box_w + 4, box_h + 4);
    pax_simple_rect(&renderer.fb, COLOR_BLACK, box_x, box_y, box_w, box_h);

    const char *title = context->is_saving_encrypted ? "Set Password to Encrypt:" : "Enter Password to Decrypt:";
    pax_draw_text(
        &renderer.fb, COLOR_WHITE, pax_font_sky_mono, 16, box_x + 10, box_y + 10, 
        title);

    // Generate masked string
    char masked_display[36]; // 32 chars max + cursor + null
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
        &renderer.fb, COLOR_WHITE, pax_font_sky_mono, 16, box_x + 10, box_y + 35, 
        masked_display);
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

    renderer.cursor_visible = true;

    return true;
}

/*----------------------------------------------------------*/

void renderer_cursor_blink(void)
{
    renderer.cursor_visible =
        !renderer.cursor_visible;
}

/*----------------------------------------------------------*/

void renderer_render(AppContext *context)
{
    if (context->redraw_request == REDRAW_NONE) 
    {
        return;
    }

    // --- FAST PATH: Cursor Blink Only ---
    if (context->redraw_request == REDRAW_CURSOR && context->state == APP_STATE_EDITOR)
    {
        renderer_update_cursor_only(&context->editor);
        renderer_present_internal();
        
        // Clear the flag and exit
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
    }

    renderer_present_internal();
    
    // Clear the flag when done
    context->redraw_request = REDRAW_NONE; 
}
