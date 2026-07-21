#ifndef EDITOR_H
#define EDITOR_H

#include <stdbool.h>
#include <stddef.h>

#include "config.h"

typedef struct
{
    /* Text storage */
    char text[EDITOR_BUFFER_SIZE];

    /* Current text length */
    size_t length;

    /* Cursor position */
    size_t cursor;

    /* Selection: anchor..cursor when active */
    size_t sel_anchor;
    bool   sel_active;

    /* Current page offset */
    size_t file_offset;

    char filename[FILE_PATH_LENGTH];

    bool modified;
    bool hex_mode;
    bool encrypted;

    char status[128];
    bool status_dirty;

} Editor;


/*------------------------------------------------------------------
 * Initialization
 *----------------------------------------------------------------*/

void editor_init(Editor *editor);

void editor_clear(Editor *editor);


/*------------------------------------------------------------------
 * Text editing
 *----------------------------------------------------------------*/

bool editor_insert(Editor *editor, char c);

bool editor_backspace(Editor *editor);

void editor_set_text(Editor *editor, const char *text);

void editor_get_text(
    const Editor *editor,
    char *dest,
    size_t size);


/*------------------------------------------------------------------
 * Cursor
 *----------------------------------------------------------------*/

/*------------------------------------------------------------------
 * Selection & clipboard support
 *----------------------------------------------------------------*/

void editor_select_all(Editor *editor);

void editor_clear_selection(Editor *editor);

/* Returns true and fills start/end (start < end) when a non-empty
 * selection exists. */
bool editor_selection(const Editor *editor, size_t *start, size_t *end);

/* Deletes the selected text; returns true when something was removed. */
bool editor_delete_selection(Editor *editor);

/* Inserts a block of text at the cursor (replacing any selection),
 * limited by the remaining buffer capacity. Returns chars inserted. */
size_t editor_insert_text(Editor *editor, const char *text, size_t length);


bool editor_move_left(Editor *editor);

bool editor_move_right(Editor *editor);

// --- The missing functions! ---
bool editor_move_up(Editor *editor);

bool editor_move_down(Editor *editor);
// ------------------------------

/* Word-wise movement (Ctrl+Left / Ctrl+Right). These do NOT touch the
 * selection, so the caller can extend a selection with them. */
bool editor_move_word_left(Editor *editor);

bool editor_move_word_right(Editor *editor);

bool editor_move_home(Editor *editor);

bool editor_move_end(Editor *editor);

size_t editor_cursor(const Editor *editor);

size_t editor_length(const Editor *editor);


/*------------------------------------------------------------------
 * File state
 *----------------------------------------------------------------*/

void editor_set_filename(
    Editor *editor,
    const char *filename);

const char *editor_filename(
    const Editor *editor);

void editor_set_offset(
    Editor *editor,
    size_t offset);

size_t editor_offset(
    const Editor *editor);


/*------------------------------------------------------------------
 * Status
 *----------------------------------------------------------------*/

void editor_update_status(Editor *editor);

const char *editor_status(
    const Editor *editor);

#endif