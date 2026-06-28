#ifndef TYPES_H
#define TYPES_H

#include <stdbool.h>
#include <stddef.h>

typedef enum
{
    STATE_EDITOR,
    STATE_MAIN_MENU,
    STATE_FILE_BROWSER
} app_state_t;

typedef struct
{
    char name[64];
    bool is_dir;
} file_entry_t;

#endif