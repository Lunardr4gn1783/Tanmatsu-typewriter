#include "app.h"

#include <string.h>
#include <stdio.h>
#include "bsp/device.h"
#include "bsp/input.h"
#include "esp_system.h"
#include "nvs.h"
#include "theme.h"
#include "renderer.h"
#include "layout.h"

/* Available editor font sizes to cycle through */
static const int font_sizes[] = {12, 16, 20, 24, 32};
static const int FONT_SIZE_COUNT = sizeof(font_sizes) / sizeof(font_sizes[0]);

#define SETTINGS_NVS_NAMESPACE "typewriter"

static void settings_save(void)
{
    nvs_handle_t handle;
    if (nvs_open(SETTINGS_NVS_NAMESPACE, NVS_READWRITE, &handle) == ESP_OK)
    {
        nvs_set_i32(handle, "font_size", app.font_size);
        nvs_set_u8(handle, "autocap", app.auto_capitalize ? 1 : 0);
        nvs_set_u8(handle, "theme", (uint8_t)app.theme_index);
        nvs_set_u8(handle, "font", (uint8_t)app.font_index);
        nvs_set_u8(handle, "wrap", app.word_wrap ? 1 : 0);
        nvs_commit(handle);
        nvs_close(handle);
    }
}

static void settings_load(void)
{
    nvs_handle_t handle;
    if (nvs_open(SETTINGS_NVS_NAMESPACE, NVS_READONLY, &handle) == ESP_OK)
    {
        int32_t fs = 0;
        uint8_t ac = 0;

        if (nvs_get_i32(handle, "font_size", &fs) == ESP_OK)
        {
            /* Only accept values from the known list */
            for (int i = 0; i < FONT_SIZE_COUNT; i++)
            {
                if (font_sizes[i] == (int)fs)
                {
                    app.font_size = (int)fs;
                    break;
                }
            }
        }

        if (nvs_get_u8(handle, "autocap", &ac) == ESP_OK)
        {
            app.auto_capitalize = (ac != 0);
        }

        uint8_t th = 0;
        if (nvs_get_u8(handle, "theme", &th) == ESP_OK && th < THEME_COUNT)
        {
            app.theme_index = (int)th;
        }

        uint8_t fo = 0;
        if (nvs_get_u8(handle, "font", &fo) == ESP_OK && fo < EDITOR_FONT_COUNT)
        {
            app.font_index = (int)fo;
        }

        uint8_t ww = 0;
        if (nvs_get_u8(handle, "wrap", &ww) == ESP_OK)
        {
            app.word_wrap = (ww != 0);
        }

        nvs_close(handle);
    }
}

static void cycle_font_size(int direction)
{
    int index = 0;
    for (int i = 0; i < FONT_SIZE_COUNT; i++)
    {
        if (font_sizes[i] == app.font_size)
        {
            index = i;
            break;
        }
    }
    index = (index + direction + FONT_SIZE_COUNT) % FONT_SIZE_COUNT;
    app.font_size = font_sizes[index];
}

/* Secure memory clearing that won't be optimized away */
static void secure_zero(void *ptr, size_t len)
{
    volatile unsigned char *p = (volatile unsigned char *)ptr;
    while (len--) *p++ = 0;
}

AppContext app;

void app_init(void)
{
    memset(&app, 0, sizeof(app));

    app.state = APP_STATE_EDITOR;

    app.cursor_visible = true;
    app.redraw_request = REDRAW_FULL;
    app.menu_selected = 0;
    app.prev_menu_selected = 0;
    app.prev_browser_selected = 0;
    app.prev_browser_scroll = 0;

    /* Setting defaults: normal size, auto-capitalize OFF, dark theme */
    app.font_size = 16;
    app.auto_capitalize = false;
    app.theme_index = 0;
    app.font_index = 0;
    app.word_wrap = true;
    settings_load();

    editor_init(&app.editor);
    filesystem_init(&app.filesystem);
    browser_init(&app.browser);
}

/*------------------------------------------------------------------
 * Input Routing: Typing Characters
 *----------------------------------------------------------------*/
/* Internal clipboard for copy/cut/paste */
static char   clipboard[EDITOR_BUFFER_SIZE];
static size_t clipboard_length = 0;

void app_toggle_md_preview(void)
{
    if (app.state == APP_STATE_EDITOR)
    {
        app.state = APP_STATE_MD_PREVIEW;
        app.md_scroll = 0;
        app.redraw_request = REDRAW_FULL;
    }
    else if (app.state == APP_STATE_MD_PREVIEW)
    {
        app.state = APP_STATE_EDITOR;
        app.redraw_request = REDRAW_FULL;
    }
}

void app_handle_shortcut(char key)
{
    if (key == 'm')
    {
        app_toggle_md_preview();
        return;
    }

    if (app.state != APP_STATE_EDITOR)
    {
        return;
    }

    size_t start, end;

    switch (key)
    {
        case 'a': /* Select all */
            editor_select_all(&app.editor);
            app.redraw_request = REDRAW_FULL;
            break;

        case 'c': /* Copy */
            if (editor_selection(&app.editor, &start, &end))
            {
                clipboard_length = end - start;
                memcpy(clipboard, app.editor.text + start, clipboard_length);
            }
            break;

        case 'x': /* Cut */
            if (editor_selection(&app.editor, &start, &end))
            {
                clipboard_length = end - start;
                memcpy(clipboard, app.editor.text + start, clipboard_length);
                editor_delete_selection(&app.editor);
                app.redraw_request = REDRAW_FULL;
            }
            break;

        case 'v': /* Paste */
            if (clipboard_length > 0)
            {
                editor_insert_text(&app.editor, clipboard, clipboard_length);
                app.redraw_request = REDRAW_FULL;
            }
            break;
    }
}

void app_handle_char(char key)
{
    if (app.state == APP_STATE_EDITOR)
    {
        bool had_selection = app.editor.sel_active;

        if (key == '\b') 
        {
            editor_backspace(&app.editor);
            app.redraw_request = REDRAW_LINE;
        } 
        else if (key == '\t')
        {
            for (int i = 0; i < TAB_WIDTH; i++)
                editor_insert(&app.editor, ' ');
            app.redraw_request = REDRAW_LINE;
        }
        else 
        {
            /* Auto-capitalize: uppercase the first letter of a sentence */
            if (app.auto_capitalize && key >= 'a' && key <= 'z')
            {
                bool sentence_start = true;
                size_t i = app.editor.cursor;

                /* Walk back over whitespace to find the previous real character */
                while (i > 0)
                {
                    char prev = app.editor.text[i - 1];

                    if (prev == ' ' || prev == '\t')
                    {
                        i--;
                        continue;
                    }

                    sentence_start = (prev == '.' || prev == '!' ||
                                      prev == '?' || prev == '\n');
                    break;
                }

                if (sentence_start)
                {
                    key = key - 'a' + 'A';
                }
            }

            editor_insert(&app.editor, key);
            app.redraw_request = had_selection ? REDRAW_FULL : REDRAW_LINE;
        }
    }
    else if (app.state == APP_STATE_SAVE_AS)
    {
        if (key == '\b') 
        {
            if (app.prompt_cursor > 0) 
            {
                app.prompt_cursor--;
                app.prompt_buffer[app.prompt_cursor] = '\0';
                app.redraw_request = REDRAW_FULL;
            }
        } 
        else if (key >= 32 && key <= 126 && key != '/') 
        {
            if (app.prompt_cursor < sizeof(app.prompt_buffer) - 1) 
            {
                app.prompt_buffer[app.prompt_cursor++] = key;
                app.prompt_buffer[app.prompt_cursor] = '\0';
                app.redraw_request = REDRAW_FULL;
            }
        }
    }
    else if (app.state == APP_STATE_PASSWORD)
    {
        if (key == '\b') 
        {
            if (app.password_cursor > 0) 
            {
                app.password_cursor--;
                app.password_buffer[app.password_cursor] = '\0';
                app.redraw_request = REDRAW_FULL;
            }
        } 
        else if (key >= 32 && key <= 126) 
        {
            if (app.password_cursor < sizeof(app.password_buffer) - 1) 
            {
                app.password_buffer[app.password_cursor++] = key;
                app.password_buffer[app.password_cursor] = '\0';
                app.redraw_request = REDRAW_FULL;
            }
        }
    }
}

/*------------------------------------------------------------------
 * Input Routing: Navigation / Special Keys
 *----------------------------------------------------------------*/
/* Move the cursor one visual line up/down. With word wrap on, a wrapped
 * segment counts as its own line, so this follows what's on screen rather
 * than what's between newlines. */
static bool move_visual(Editor *editor, int direction)
{
    if (!app.word_wrap)
    {
        return (direction < 0) ? editor_move_up(editor) : editor_move_down(editor);
    }

    renderer_rebuild_layout(editor);
    const Layout *lay = renderer_layout();

    int line = layout_line_at(lay, editor->cursor);
    int target = line + direction;

    if (target < 0 || target >= lay->count)
    {
        return false;
    }

    size_t column = editor->cursor - lay->lines[line].start;
    size_t target_len = lay->lines[target].end - lay->lines[target].start;

    if (column > target_len)
    {
        column = target_len;
    }

    editor->cursor = lay->lines[target].start + column;
    editor->status_dirty = true;
    return true;
}

/* Starts or extends a selection when Shift is held, and drops it when
 * it isn't. Call before any cursor movement. */
static void nav_selection_prepare(bool shift)
{
    if (shift)
    {
        if (!app.editor.sel_active)
        {
            app.editor.sel_anchor = app.editor.cursor;
            app.editor.sel_active = true;
        }
    }
    else
    {
        editor_clear_selection(&app.editor);
    }
}

void app_handle_nav(uint32_t nav_key, uint32_t modifiers)
{
    bool shift __attribute__((unused)) =
        (modifiers & BSP_INPUT_MODIFIER_SHIFT) != 0;
    bool ctrl __attribute__((unused)) =
        (modifiers & BSP_INPUT_MODIFIER_CTRL) != 0;

    switch (app.state)
    {
        /*==========================================================
            MARKDOWN PREVIEW STATE
        ==========================================================*/
        case APP_STATE_MD_PREVIEW:
            if (nav_key == BSP_INPUT_NAVIGATION_KEY_ESC ||
                nav_key == BSP_INPUT_NAVIGATION_KEY_F2)
            {
                app.state = APP_STATE_EDITOR;
                app.redraw_request = REDRAW_FULL;
            }
            else if (nav_key == BSP_INPUT_NAVIGATION_KEY_UP)
            {
                app.md_scroll -= 3 * (app.font_size + 4);
                if (app.md_scroll < 0) app.md_scroll = 0;
                app.redraw_request = REDRAW_FULL;
            }
            else if (nav_key == BSP_INPUT_NAVIGATION_KEY_DOWN)
            {
                app.md_scroll += 3 * (app.font_size + 4);
                app.redraw_request = REDRAW_FULL;
            }
            break;

        /*==========================================================
            EDITOR STATE
        ==========================================================*/
        case APP_STATE_EDITOR:
            if (nav_key == BSP_INPUT_NAVIGATION_KEY_F2 || nav_key == BSP_INPUT_NAVIGATION_KEY_ESC) 
            {
                app.state = APP_STATE_MENU;
                app.menu_selected = 0;
                app.redraw_request = REDRAW_FULL;
            } 
            else if (nav_key == BSP_INPUT_NAVIGATION_KEY_RETURN)
            {
                editor_insert(&app.editor, '\n');
                app.redraw_request = REDRAW_FULL;
            }
            else if (nav_key == BSP_INPUT_NAVIGATION_KEY_LEFT)
            {
                nav_selection_prepare(shift);

                bool moved = ctrl ? editor_move_word_left(&app.editor)
                                  : editor_move_left(&app.editor);

                if (moved)
                    app.redraw_request = shift ? REDRAW_FULL : REDRAW_LINE;
            }
            else if (nav_key == BSP_INPUT_NAVIGATION_KEY_RIGHT)
            {
                nav_selection_prepare(shift);

                bool moved = ctrl ? editor_move_word_right(&app.editor)
                                  : editor_move_right(&app.editor);

                if (moved)
                    app.redraw_request = shift ? REDRAW_FULL : REDRAW_LINE;
            }
            else if (nav_key == BSP_INPUT_NAVIGATION_KEY_UP)
            {
                nav_selection_prepare(shift);
                if (move_visual(&app.editor, -1)) app.redraw_request = REDRAW_FULL;
            }
            else if (nav_key == BSP_INPUT_NAVIGATION_KEY_DOWN)
            {
                nav_selection_prepare(shift);
                if (move_visual(&app.editor, 1)) app.redraw_request = REDRAW_FULL;
            }
            else if (nav_key == BSP_INPUT_NAVIGATION_KEY_VOLUME_DOWN)
            {
                if (strlen(app.filesystem.active_file) > 0) 
                {
                    app.filesystem.file_offset += PAGE_SIZE;
                    filesystem_load_file(&app.filesystem, &app.editor);
                    app.redraw_request = REDRAW_FULL;
                }
            }
            else if (nav_key == BSP_INPUT_NAVIGATION_KEY_VOLUME_UP)
            {
                if (strlen(app.filesystem.active_file) > 0 && app.filesystem.file_offset > 0) 
                {
                    if (app.filesystem.file_offset >= PAGE_SIZE) {
                        app.filesystem.file_offset -= PAGE_SIZE;
                    } else {
                        app.filesystem.file_offset = 0;
                    }
                    filesystem_load_file(&app.filesystem, &app.editor);
                    app.redraw_request = REDRAW_FULL;
                }
            }
            break;

        /*==========================================================
            MENU STATE
        ==========================================================*/
        case APP_STATE_MENU:
            if (nav_key == BSP_INPUT_NAVIGATION_KEY_DOWN) 
            {
                app.prev_menu_selected = app.menu_selected;
                app.menu_selected++;
                if (app.menu_selected >= MENU_ITEM_COUNT)
                    app.menu_selected = 0;
                app.redraw_request = REDRAW_MENU;
            }
            else if (nav_key == BSP_INPUT_NAVIGATION_KEY_UP) 
            {
                app.prev_menu_selected = app.menu_selected;
                app.menu_selected--;
                if (app.menu_selected < 0)
                    app.menu_selected = MENU_ITEM_COUNT - 1;
                app.redraw_request = REDRAW_MENU;
            }
            else if (nav_key == BSP_INPUT_NAVIGATION_KEY_ESC || nav_key == BSP_INPUT_NAVIGATION_KEY_F2) 
            {
                app.state = APP_STATE_EDITOR;
                app.redraw_request = REDRAW_FULL;
            }
            else if (nav_key == BSP_INPUT_NAVIGATION_KEY_LEFT)
            {
                /* Left arrow cycles settings backwards */
                if (app.menu_selected == 6) // Text Size
                {
                    cycle_font_size(-1);
                    settings_save();
                    app.redraw_request = REDRAW_FULL;
                }
                else if (app.menu_selected == 7) // Font
                {
                    app.font_index = (app.font_index + EDITOR_FONT_COUNT - 1) % EDITOR_FONT_COUNT;
                    settings_save();
                    app.redraw_request = REDRAW_FULL;
                }
                else if (app.menu_selected == 10) // Theme
                {
                    app.theme_index = (app.theme_index + THEME_COUNT - 1) % THEME_COUNT;
                    settings_save();
                    app.redraw_request = REDRAW_FULL;
                }
            }
            else if (nav_key == BSP_INPUT_NAVIGATION_KEY_RETURN || nav_key == BSP_INPUT_NAVIGATION_KEY_RIGHT) 
            {
                switch (app.menu_selected) 
                {
                    case 0: // Save (Overwrite)
                        app.filesystem.save_mode = true;
                        filesystem_load_directory(&app.filesystem, &app.browser);
                        app.state = APP_STATE_BROWSER;
                        break;
                    case 1: // Save As... (New File)
                        memset(app.prompt_buffer, 0, sizeof(app.prompt_buffer));
                        app.prompt_cursor = 0;
                        app.state = APP_STATE_SAVE_AS;
                        break;
                    case 2: // Save Encrypted
                        app.selected_cipher = 0; // Start at top of cipher list
                        app.is_saving_encrypted = true;
                        app.state = APP_STATE_CRYPTO_SELECT;
                        break;
                    case 3: // Load Text
                        app.filesystem.save_mode = false;
                        app.filesystem.hex_mode = false;
                        filesystem_load_directory(&app.filesystem, &app.browser);
                        app.state = APP_STATE_BROWSER;
                        break;
                    case 4: // Load Encrypted
                        app.filesystem.save_mode = false;
                        app.filesystem.hex_mode = false;
                        app.is_saving_encrypted = false;
                        app.is_loading_encrypted = true;
                        
                        filesystem_load_directory(&app.filesystem, &app.browser);
                        app.state = APP_STATE_BROWSER;
                        break;
                    case 5: // Load Hex
                        app.filesystem.save_mode = false;
                        app.filesystem.hex_mode = true;
                        filesystem_load_directory(&app.filesystem, &app.browser);
                        app.state = APP_STATE_BROWSER;
                        break;
                    case 6: // Text Size (cycle 12 -> 16 -> 20 -> 24 -> 32)
                        cycle_font_size(1);
                        settings_save();
                        break;
                    case 7: // Font (cycle through PAX built-in fonts)
                        app.font_index = (app.font_index + 1) % EDITOR_FONT_COUNT;
                        settings_save();
                        break;
                    case 8: // Word Wrap (toggle, default on)
                        app.word_wrap = !app.word_wrap;
                        settings_save();
                        break;
                    case 9: // Auto Capitals (toggle, default off)
                        app.auto_capitalize = !app.auto_capitalize;
                        settings_save();
                        break;
                    case 10: // Theme (cycle through color themes)
                        app.theme_index = (app.theme_index + 1) % THEME_COUNT;
                        settings_save();
                        break;
                    case 11: // Markdown Preview
                        app.state = APP_STATE_MD_PREVIEW;
                        app.md_scroll = 0;
                        break;
                    case 12: // Back to Editor
                        app.state = APP_STATE_EDITOR;
                        break;
                    case 13: // Quit to Launcher
                        bsp_device_restart_to_launcher();
                        break;
                }
                app.redraw_request = REDRAW_FULL;
            }
            break;

        /*==========================================================
            BROWSER STATE
        ==========================================================*/
        case APP_STATE_BROWSER:
            if (nav_key == BSP_INPUT_NAVIGATION_KEY_DOWN) 
            {
                app.prev_browser_selected = app.browser.selected;
                app.prev_browser_scroll = app.browser.scroll;
                if (browser_move_down(&app.browser)) app.redraw_request = REDRAW_BROWSER;
            } 
            else if (nav_key == BSP_INPUT_NAVIGATION_KEY_UP) 
            {
                app.prev_browser_selected = app.browser.selected;
                app.prev_browser_scroll = app.browser.scroll;
                if (browser_move_up(&app.browser)) app.redraw_request = REDRAW_BROWSER;
            } 
            else if (nav_key == BSP_INPUT_NAVIGATION_KEY_ESC || nav_key == BSP_INPUT_NAVIGATION_KEY_LEFT) 
            {
                app.state = APP_STATE_MENU;
                app.redraw_request = REDRAW_FULL;
            } 
            else if (nav_key == BSP_INPUT_NAVIGATION_KEY_RETURN || nav_key == BSP_INPUT_NAVIGATION_KEY_RIGHT) 
            {
                const BrowserEntry *selected = browser_selected(&app.browser);
                
                if (selected != NULL) 
                {
                    if (selected->is_directory) 
                    {
                        if (strcmp(selected->name, "..") == 0) 
                        {
                            filesystem_parent_directory(&app.filesystem);
                        } 
                        else 
                        {
                            filesystem_enter_directory(&app.filesystem, selected->name);
                        }
                        filesystem_load_directory(&app.filesystem, &app.browser);
                    } 
                    else 
                    {
                        filesystem_select_file(&app.filesystem, selected);
                        
                        if (app.filesystem.save_mode) 
                        {
                            filesystem_save_file(&app.filesystem, &app.editor);
                            app.state = APP_STATE_EDITOR;
                        } 
                        else if (app.is_loading_encrypted) 
                        {
                            // Load Encrypted mode - divert to cipher selection first
                            app.selected_cipher = 0;
                            app.state = APP_STATE_CRYPTO_SELECT; 
                        }
                        else 
                        {
                            filesystem_load_file(&app.filesystem, &app.editor);
                            app.state = APP_STATE_EDITOR;
                        }
                    }
                    app.redraw_request = REDRAW_FULL;
                }
            }
            break;

        /*==========================================================
            CRYPTO SELECT STATE
        ==========================================================*/
        case APP_STATE_CRYPTO_SELECT:
            if (nav_key == BSP_INPUT_NAVIGATION_KEY_DOWN) 
            {
                // Prevent scrolling past the available methods
                if (app.selected_cipher < CRYPTO_METHOD_COUNT - 1) 
                {
                    app.selected_cipher++;
                    app.redraw_request = REDRAW_FULL;
                }
            }
            else if (nav_key == BSP_INPUT_NAVIGATION_KEY_UP) 
            {
                if (app.selected_cipher > 0) 
                {
                    app.selected_cipher--;
                    app.redraw_request = REDRAW_FULL;
                }
            }
            else if (nav_key == BSP_INPUT_NAVIGATION_KEY_ESC || nav_key == BSP_INPUT_NAVIGATION_KEY_LEFT) 
            {
                // Dynamic back-routing based on intent
                if (app.is_saving_encrypted) {
                    app.state = APP_STATE_MENU;
                } else {
                    app.state = APP_STATE_BROWSER;
                }
                app.redraw_request = REDRAW_FULL;
            }
            else if (nav_key == BSP_INPUT_NAVIGATION_KEY_RETURN || nav_key == BSP_INPUT_NAVIGATION_KEY_RIGHT) 
            {
                // Lock in the algorithm and prompt for password
                secure_zero(app.password_buffer, sizeof(app.password_buffer));
                app.password_cursor = 0;
                app.state = APP_STATE_PASSWORD;
                app.redraw_request = REDRAW_FULL;
            }
            break;

        /*==========================================================
            SAVE AS STATE (Prompt)
        ==========================================================*/
        case APP_STATE_SAVE_AS:
            if (nav_key == BSP_INPUT_NAVIGATION_KEY_ESC) 
            {
                app.state = APP_STATE_EDITOR;
                app.redraw_request = REDRAW_FULL;
            } 
            else if (nav_key == BSP_INPUT_NAVIGATION_KEY_RETURN) 
            {
                if (app.prompt_cursor > 0) 
                {
                    snprintf(
                        app.filesystem.active_file, 
                        sizeof(app.filesystem.active_file), 
                        "%s/%s", 
                        app.filesystem.current_path, 
                        app.prompt_buffer);
                    
                    app.filesystem.file_offset = 0;
                    app.editor.file_offset = 0;

                    filesystem_save_file(&app.filesystem, &app.editor);
                    
                    editor_set_filename(&app.editor, app.filesystem.active_file);
                    editor_update_status(&app.editor);

                    app.state = APP_STATE_EDITOR;
                    app.redraw_request = REDRAW_FULL;
                }
            }
            break;

        /*==========================================================
            PASSWORD STATE (Prompt)
        ==========================================================*/
        case APP_STATE_PASSWORD:
            if (nav_key == BSP_INPUT_NAVIGATION_KEY_ESC) 
            {
                app.is_loading_encrypted = false;
                app.state = APP_STATE_EDITOR;
                app.redraw_request = REDRAW_FULL;
            } 
            else if (nav_key == BSP_INPUT_NAVIGATION_KEY_RETURN) 
            {
                if (app.password_cursor > 0) 
                {
                    // Execute using the dynamically selected cipher!
                    if (app.is_saving_encrypted) 
                    {
                        filesystem_save_encrypted(&app.filesystem, &app.editor, app.password_buffer, app.selected_cipher);
                    } 
                    else 
                    {
                        filesystem_load_encrypted(&app.filesystem, &app.editor, app.password_buffer, app.selected_cipher);
                    }

                    secure_zero(app.password_buffer, sizeof(app.password_buffer));
                    app.password_cursor = 0;
                    app.is_loading_encrypted = false;
                    
                    app.state = APP_STATE_EDITOR;
                    app.redraw_request = REDRAW_FULL;
                }
            }
            break;
    }
}