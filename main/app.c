#include "app.h"

#include <string.h>
#include <stdio.h>
#include "bsp/input.h"

AppContext app;

void app_init(void)
{
    memset(&app, 0, sizeof(app));

    app.state = APP_STATE_EDITOR;

    app.cursor_visible = true;
    app.redraw_request = REDRAW_FULL;
    app.menu_selected = 0;

    editor_init(&app.editor);
    filesystem_init(&app.filesystem);
    browser_init(&app.browser);
}

/*------------------------------------------------------------------
 * Input Routing: Typing Characters
 *----------------------------------------------------------------*/
void app_handle_char(char key)
{
    if (app.state == APP_STATE_EDITOR)
    {
        if (key == '\b') 
        {
            editor_backspace(&app.editor);
        } 
        else 
        {
            editor_insert(&app.editor, key);
        }
        
        app.redraw_request = REDRAW_FULL; 
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
void app_handle_nav(uint32_t nav_key)
{
    switch (app.state)
    {
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
            else if (nav_key == BSP_INPUT_NAVIGATION_KEY_F4) 
            {
                filesystem_save_file(&app.filesystem, &app.editor);
                app.redraw_request = REDRAW_FULL;
            }
            else if (nav_key == BSP_INPUT_NAVIGATION_KEY_LEFT)
            {
                if (editor_move_left(&app.editor)) app.redraw_request = REDRAW_CURSOR;
            }
            else if (nav_key == BSP_INPUT_NAVIGATION_KEY_RIGHT)
            {
                if (editor_move_right(&app.editor)) app.redraw_request = REDRAW_CURSOR;
            }
            else if (nav_key == BSP_INPUT_NAVIGATION_KEY_UP)
            {
                if (editor_move_up(&app.editor)) app.redraw_request = REDRAW_CURSOR;
            }
            else if (nav_key == BSP_INPUT_NAVIGATION_KEY_DOWN)
            {
                if (editor_move_down(&app.editor)) app.redraw_request = REDRAW_CURSOR;
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
                if (app.menu_selected < 6) 
                {
                    app.menu_selected++;
                    app.redraw_request = REDRAW_FULL;
                }
            }
            else if (nav_key == BSP_INPUT_NAVIGATION_KEY_UP) 
            {
                if (app.menu_selected > 0) 
                {
                    app.menu_selected--;
                    app.redraw_request = REDRAW_FULL;
                }
            }
            else if (nav_key == BSP_INPUT_NAVIGATION_KEY_ESC || nav_key == BSP_INPUT_NAVIGATION_KEY_F2) 
            {
                app.state = APP_STATE_EDITOR;
                app.redraw_request = REDRAW_FULL;
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
                        
                        filesystem_load_directory(&app.filesystem, &app.browser);
                        app.state = APP_STATE_BROWSER;
                        break;
                    case 5: // Load Hex
                        app.filesystem.save_mode = false;
                        app.filesystem.hex_mode = true;
                        filesystem_load_directory(&app.filesystem, &app.browser);
                        app.state = APP_STATE_BROWSER;
                        break;
                    case 6: // Back to Editor
                        app.state = APP_STATE_EDITOR;
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
                if (browser_move_down(&app.browser)) app.redraw_request = REDRAW_FULL;
            } 
            else if (nav_key == BSP_INPUT_NAVIGATION_KEY_UP) 
            {
                if (browser_move_up(&app.browser)) app.redraw_request = REDRAW_FULL;
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
                        else if (!app.filesystem.save_mode && app.menu_selected == 4) 
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
                memset(app.password_buffer, 0, sizeof(app.password_buffer));
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

                    memset(app.password_buffer, 0, sizeof(app.password_buffer));
                    
                    app.state = APP_STATE_EDITOR;
                    app.redraw_request = REDRAW_FULL;
                }
            }
            break;
    }
}