#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <stdbool.h>
#include <stddef.h>

#include "browser.h"
#include "editor.h"
#include "config.h"
#include "crypto.h"

typedef struct
{
    /* Current directory */

    char current_path[DIRECTORY_PATH_LENGTH];

    /* Currently opened file */

    char active_file[FILE_PATH_LENGTH];

    /* Paging offset */

    size_t file_offset;

    /* Browser mode */

    bool save_mode;

    bool hex_mode;

    /* Filesystem state */

    bool mounted;

} Filesystem;


/* Initialization */

void filesystem_init(Filesystem *fs);


/* Mount state */

bool filesystem_is_mounted(
    const Filesystem *fs);

void filesystem_set_mounted(
    Filesystem *fs,
    bool mounted);


/* Directory */

bool filesystem_load_directory(
    Filesystem *fs,
    Browser *browser);

bool filesystem_enter_directory(
    Filesystem *fs,
    const char *directory);

bool filesystem_parent_directory(
    Filesystem *fs);


/* Files */

bool filesystem_load_file(
    Filesystem *fs,
    Editor *editor);

bool filesystem_save_file(
    Filesystem *fs,
    const Editor *editor);


/* Selection */

bool filesystem_select_file(
    Filesystem *fs,
    const BrowserEntry *entry);


/* Path helpers */

const char *
filesystem_current_path(
    const Filesystem *fs);

const char *
filesystem_active_file(
    const Filesystem *fs);

bool filesystem_load_encrypted(
    Filesystem *fs, 
    Editor *editor, 
    const char *password, 
    CryptoMethod method);

bool filesystem_save_encrypted(
    Filesystem *fs, 
    Editor *editor, 
    const char *password, 
    CryptoMethod method);

#endif