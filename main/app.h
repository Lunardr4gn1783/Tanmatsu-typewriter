#ifndef APP_H
#define APP_H

#include <stdint.h>
#include <stdbool.h>

#include "editor.h"
#include "browser.h"
#include "filesystem.h"
#include "crypto.h"

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
    REDRAW_LINE,    // Future optimization: only redraw current line
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

    int menu_selected;

    bool cursor_visible;

    RedrawType redraw_request;

    CryptoMethod selected_cipher;

} AppContext;

extern AppContext app;

void app_init(void);

void app_handle_char(char key);

void app_handle_nav(uint32_t key);

#endif