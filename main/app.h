#ifndef APP_H
#define APP_H

#include <stdint.h>
#include <stdbool.h>

#include "editor.h"
#include "browser.h"
#include "filesystem.h"
#include "crypto.h"

// Total number of items in the main menu (app.c + renderer.c stay in sync)
#define MENU_ITEM_COUNT 11

// The core application states
typedef enum
{
    APP_STATE_EDITOR,
    APP_STATE_MENU,
    APP_STATE_BROWSER,
    APP_STATE_SAVE_AS,
    APP_STATE_PASSWORD,
    APP_STATE_CRYPTO_SELECT

} AppState;

// The partial redraw optimization flags
typedef enum 
{
    REDRAW_NONE = 0,
    REDRAW_CURSOR,  // Only the cursor toggled
    REDRAW_LINE,    // Only the current editor line changed
    REDRAW_MENU,    // Only the menu selection highlight moved
    REDRAW_BROWSER, // Only the browser selection highlight moved
    REDRAW_FULL     // Page changed, menu opened, etc.
} RedrawType;

// The master state container
typedef struct
{
    AppState state;

    Editor editor;

    Browser browser;

    Filesystem filesystem;

    // Buffer to hold the filename as the user types it
    char prompt_buffer[64];
    size_t prompt_cursor;

    // Buffer to hold the password as the user types it
    char password_buffer[32];
    size_t password_cursor;
    
    // Tracks intent when returning from the password prompt
    bool is_saving_encrypted;
    bool is_loading_encrypted;

    int menu_selected;
    int prev_menu_selected;

    int prev_browser_selected;
    int prev_browser_scroll;

    bool cursor_visible;

    bool caps_lock;

    /* User settings (persisted in NVS) */
    int  font_size;         // Editor text size in pixels
    bool auto_capitalize;   // Capitalize first letter of sentences (default: off)
    int  theme_index;       // Active color theme (see theme.h)

    RedrawType redraw_request;

    CryptoMethod selected_cipher;

} AppContext;

extern AppContext app;

void app_init(void);

void app_handle_char(char key);

void app_handle_nav(uint32_t key);

#endif