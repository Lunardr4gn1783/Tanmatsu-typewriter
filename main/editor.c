#include "editor.h"

#include <stdio.h>
#include <string.h>

/*------------------------------------------------------------------
 * Private Helpers
 *----------------------------------------------------------------*/

static void editor_mark_modified(Editor *editor)
{
    editor->modified = true;
    editor->status_dirty = true;
}

static void editor_mark_clean(Editor *editor)
{
    editor->modified = false;
    editor->status_dirty = true;
}

/*------------------------------------------------------------------
 * Initialization
 *----------------------------------------------------------------*/

void editor_init(Editor *editor)
{
    if (editor == NULL)
    {
        return;
    }

    memset(editor, 0, sizeof(Editor));

    editor->length = 0;
    editor->cursor = 0;

    editor_update_status(editor);
}

void editor_clear(Editor *editor)
{
    if (editor == NULL)
    {
        return;
    }

    memset(editor->text, 0, sizeof(editor->text));

    editor->length = 0;
    editor->cursor = 0;

    editor_mark_clean(editor);
    editor_update_status(editor);
}

/*------------------------------------------------------------------
 * Text Editing
 *----------------------------------------------------------------*/


/*------------------------------------------------------------------
 * Selection & clipboard support
 *----------------------------------------------------------------*/

void editor_select_all(Editor *editor)
{
    if (editor == NULL) return;

    editor->sel_anchor = 0;
    editor->cursor     = editor->length;
    editor->sel_active = (editor->length > 0);
    editor->status_dirty = true;
}

void editor_clear_selection(Editor *editor)
{
    if (editor == NULL) return;
    editor->sel_active = false;
}

bool editor_selection(const Editor *editor, size_t *start, size_t *end)
{
    if (editor == NULL || !editor->sel_active) return false;
    if (editor->sel_anchor == editor->cursor) return false;

    if (editor->sel_anchor < editor->cursor)
    {
        *start = editor->sel_anchor;
        *end   = editor->cursor;
    }
    else
    {
        *start = editor->cursor;
        *end   = editor->sel_anchor;
    }
    return true;
}

bool editor_delete_selection(Editor *editor)
{
    size_t start, end;

    if (!editor_selection(editor, &start, &end)) return false;

    memmove(
        &editor->text[start],
        &editor->text[end],
        editor->length - end + 1); // +1 includes the null terminator

    editor->length -= (end - start);
    editor->cursor  = start;
    editor->sel_active = false;

    editor_mark_modified(editor);
    return true;
}

size_t editor_insert_text(Editor *editor, const char *text, size_t length)
{
    if (editor == NULL || text == NULL) return 0;

    editor_delete_selection(editor);

    size_t room = (EDITOR_BUFFER_SIZE - 1) - editor->length;
    if (length > room) length = room;
    if (length == 0) return 0;

    memmove(
        &editor->text[editor->cursor + length],
        &editor->text[editor->cursor],
        editor->length - editor->cursor + 1);

    memcpy(&editor->text[editor->cursor], text, length);

    editor->cursor += length;
    editor->length += length;

    editor_mark_modified(editor);
    return length;
}

bool editor_insert(Editor *editor, char c)
{
    if (editor == NULL)
    {
        return false;
    }

    /* Only accept printable characters and newlines */
    if (c != '\n' && (c < 32 || c > 126))
    {
        return false;
    }

    /* Typing replaces the current selection */
    editor_delete_selection(editor);

    if (editor->length >= (EDITOR_BUFFER_SIZE - 1))
    {
        return false;
    }

    // Shift text to the right to make room for the new character
    memmove(
        &editor->text[editor->cursor + 1],
        &editor->text[editor->cursor],
        editor->length - editor->cursor + 1); // +1 includes the null terminator

    editor->text[editor->cursor] = c;
    editor->cursor++;
    editor->length++;

    editor_mark_modified(editor);

    return true;
}

bool editor_backspace(Editor *editor)
{
    if (editor == NULL)
    {
        return false;
    }

    /* With a selection, backspace deletes the selection */
    if (editor_delete_selection(editor))
    {
        return true;
    }

    if (editor->cursor == 0)
    {
        return false;
    }

    // Shift text to the left to close the gap
    memmove(
        &editor->text[editor->cursor - 1],
        &editor->text[editor->cursor],
        editor->length - editor->cursor + 1); // +1 includes the null terminator

    editor->cursor--;
    editor->length--;

    editor_mark_modified(editor);

    return true;
}

void editor_set_text(Editor *editor, const char *text)
{
    if ((editor == NULL) || (text == NULL))
    {
        return;
    }

    strncpy(editor->text,
        text,
        EDITOR_BUFFER_SIZE - 1);

    editor->text[EDITOR_BUFFER_SIZE - 1] = '\0';

    editor->length = strlen(editor->text);
    editor->cursor = editor->length;

    editor_mark_clean(editor);
    editor_update_status(editor);
}

void editor_get_text(
    const Editor *editor,
    char *dest,
    size_t size)
{
    if ((editor == NULL) ||
        (dest == NULL) ||
        (size == 0))
    {
        return;
    }

    strncpy(dest,
            editor->text,
            size - 1);

    dest[size - 1] = '\0';
}

/*------------------------------------------------------------------
 * Cursor
 *----------------------------------------------------------------*/

bool editor_move_left(Editor *editor)
{
    if (editor == NULL || editor->cursor == 0)
    {
        return false;
    }

    editor->cursor--;
    editor->status_dirty = true;
    return true;
}

bool editor_move_right(Editor *editor)
{
    if (editor == NULL || editor->cursor >= editor->length)
    {
        return false;
    }

    editor->cursor++;
    editor->status_dirty = true;
    return true;
}

bool editor_move_up(Editor *editor)
{
    if (editor == NULL || editor->cursor == 0) return false;

    // 1. Find the start of the current line
    size_t curr_start = editor->cursor;
    while (curr_start > 0 && editor->text[curr_start - 1] != '\n') 
    {
        curr_start--;
    }
    
    // 2. Calculate our current horizontal column
    size_t col = editor->cursor - curr_start;
    
    if (curr_start == 0) 
    {
        editor->cursor = 0; // Already on the first line
        return true;
    }
    
    // 3. Find the start of the previous line
    size_t prev_start = curr_start - 1;
    while (prev_start > 0 && editor->text[prev_start - 1] != '\n') 
    {
        prev_start--;
    }
    
    // 4. Calculate the length of the previous line
    size_t prev_len = (curr_start - 1) - prev_start;
    
    // 5. Snap to the end of the line if it's shorter than our current column
    if (col > prev_len) col = prev_len; 
    
    editor->cursor = prev_start + col;
    editor->status_dirty = true;
    return true;
}

bool editor_move_down(Editor *editor)
{
    if (editor == NULL || editor->cursor >= editor->length) return false;
    
    // 1. Find the start of the current line to get our column
    size_t curr_start = editor->cursor;
    while (curr_start > 0 && editor->text[curr_start - 1] != '\n') 
    {
        curr_start--;
    }
    size_t col = editor->cursor - curr_start;
    
    // 2. Find the start of the next line
    size_t next_start = editor->cursor;
    while (next_start < editor->length && editor->text[next_start] != '\n') 
    {
        next_start++;
    }
    
    if (next_start >= editor->length) 
    {
        editor->cursor = editor->length; // Already on the last line
        return true;
    }
    next_start++; // Jump over the '\n' character
    
    // 3. Find the length of the next line
    size_t next_end = next_start;
    while (next_end < editor->length && editor->text[next_end] != '\n') 
    {
        next_end++;
    }
    size_t next_len = next_end - next_start;
    
    // 4. Snap to the end of the line if it's shorter than our current column
    if (col > next_len) col = next_len;
    
    editor->cursor = next_start + col;
    editor->status_dirty = true;
    return true;
}

static bool is_word_char(char c)
{
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') ||
           c == '_';
}

bool editor_move_word_left(Editor *editor)
{
    if (editor == NULL || editor->cursor == 0) return false;

    size_t p = editor->cursor;

    /* Skip any separators directly before the cursor */
    while (p > 0 && !is_word_char(editor->text[p - 1])) p--;

    /* Then skip the word itself */
    while (p > 0 && is_word_char(editor->text[p - 1])) p--;

    editor->cursor = p;
    editor->status_dirty = true;
    return true;
}

bool editor_move_word_right(Editor *editor)
{
    if (editor == NULL || editor->cursor >= editor->length) return false;

    size_t p = editor->cursor;

    while (p < editor->length && !is_word_char(editor->text[p])) p++;
    while (p < editor->length && is_word_char(editor->text[p])) p++;

    editor->cursor = p;
    editor->status_dirty = true;
    return true;
}

bool editor_move_home(Editor *editor)
{
    if (editor == NULL) return false;
    editor_clear_selection(editor);
    editor->cursor = 0;
    editor->status_dirty = true;
    return true;
}

bool editor_move_end(Editor *editor)
{
    if (editor == NULL) return false;
    editor_clear_selection(editor);
    editor->cursor = editor->length;
    editor->status_dirty = true;
    return true;
}

size_t editor_cursor(const Editor *editor)
{
    if (editor == NULL) return 0;
    return editor->cursor;
}

size_t editor_length(const Editor *editor)
{
    if (editor == NULL) return 0;
    return editor->length;
}

/*------------------------------------------------------------------
 * File State
 *----------------------------------------------------------------*/

void editor_set_filename(
    Editor *editor,
    const char *filename)
{
    if ((editor == NULL) || (filename == NULL)) return;

    strncpy(editor->filename,
            filename,
            FILE_PATH_LENGTH - 1);

    editor->filename[FILE_PATH_LENGTH - 1] = '\0';
    editor->status_dirty = true;
}

const char *editor_filename(
    const Editor *editor)
{
    if (editor == NULL) return "";
    return editor->filename;
}

void editor_set_offset(
    Editor *editor,
    size_t offset)
{
    if (editor == NULL) return;
    editor->file_offset = offset;
    editor->status_dirty = true;
}

size_t editor_offset(
    const Editor *editor)
{
    if (editor == NULL) return 0;
    return editor->file_offset;
}

/*------------------------------------------------------------------
 * Status
 *----------------------------------------------------------------*/

void editor_update_status(Editor *editor)
{
    if (editor == NULL)
    {
        return;
    }

    // Extract just the filename from the full path to save screen space
    const char *display_name = editor->filename;
    const char *last_slash = strrchr(editor->filename, '/');
    if (last_slash != NULL) 
    {
        display_name = last_slash + 1; // Point to the character right after the '/'
    }

    // Provide a fallback if no file is loaded yet
    if (display_name[0] == '\0') 
    {
        display_name = "Untitled";
    }

    // %.64s explicitly tells the compiler it will write a max of 64 characters
    snprintf(
        editor->status,
        sizeof(editor->status),
        "Pg:%u  Size:%u/%u  %s%.64s", 
        (unsigned)(editor->file_offset / PAGE_SIZE),
        (unsigned)editor->length,
        (unsigned)(EDITOR_BUFFER_SIZE - 1),
        editor->modified ? "*" : "",
        display_name);

    editor->status_dirty = false;
}

const char *editor_status(
    const Editor *editor)
{
    if (editor == NULL) return "";
    return editor->status;
}
