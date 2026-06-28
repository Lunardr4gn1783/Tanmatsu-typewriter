#ifndef CONFIG_H
#define CONFIG_H

#include <stddef.h>

/*=========================================================================
    Application
=========================================================================*/

#define APP_NAME                "Tanmatsu Editor"
#define APP_VERSION             "1.2.0"

/*=========================================================================
    Storage
=========================================================================*/

#define SD_CARD_MOUNT_POINT     "/sdcard"

/*=========================================================================
    Editor
=========================================================================*/

/* Maximum text held in memory at one time */
#define EDITOR_BUFFER_SIZE       2048

/* Number of bytes loaded per page */
#define PAGE_SIZE               (EDITOR_BUFFER_SIZE - 1)

/*=========================================================================
    File Browser
=========================================================================*/

#define MAX_DIRECTORY_ENTRIES   128

#define FILE_NAME_LENGTH         64

#define DIRECTORY_PATH_LENGTH   256

#define FILE_PATH_LENGTH        512

/*=========================================================================
    User Interface
=========================================================================*/

#define STATUS_BAR_HEIGHT        20

#define MENU_ITEM_HEIGHT         20

#define HEADER_HEIGHT            42

#define SCROLLBAR_WIDTH           6

#define FONT_HEIGHT              16

#define LINE_SPACING             18

/*=========================================================================
    Cursor
=========================================================================*/

#define CURSOR_WIDTH              2

#define CURSOR_BLINK_MS         500

/*=========================================================================
    Filesystem
=========================================================================*/

#define MAX_OPEN_FILES            5

#define ALLOCATION_UNIT_SIZE  (16 * 1024)

/*=========================================================================
    Colors (ARGB8888)
=========================================================================*/

#define COLOR_BLACK      0xFF000000
#define COLOR_WHITE      0xFFFFFFFF

#define COLOR_RED        0xFFFF0000

#define COLOR_BLUE       0xFF0055FF

#define COLOR_GRAY       0xFF888888

#define COLOR_BG_HEADER  0xFF222222

#define COLOR_BG_ROW1    0xFF111111

#define COLOR_BG_ROW2    0xFF1A1A1A

#define COLOR_SCROLL_BG  0xFF333333

#define COLOR_SCROLL_FG  0xFF888888

#endif