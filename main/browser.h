#ifndef BROWSER_H
#define BROWSER_H

#include <stdbool.h>
#include <stddef.h>

#include "config.h"

typedef struct
{
    char name[FILE_NAME_LENGTH];

    char full_path[FILE_PATH_LENGTH];

    size_t size;

    bool is_directory;

} BrowserEntry;

typedef struct
{
    BrowserEntry entries[MAX_DIRECTORY_ENTRIES];

    int count;

    int selected;

    int scroll;

} Browser;


/* Initialization */

void browser_init(Browser *browser);

void browser_clear(Browser *browser);

void browser_reset(Browser *browser);


/* Entry Management */

BrowserEntry *
browser_add_entry(Browser *browser);

const BrowserEntry *
browser_selected(const Browser *browser);


/* Navigation */

bool browser_move_up(Browser *browser);

bool browser_move_down(Browser *browser);

#endif