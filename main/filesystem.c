#include "filesystem.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "esp_log.h"
#include "config.h"
#include "crypto.h" // Added crypto module

static const char *TAG = "filesystem";

/*----------------------------------------------------------
 * Private Helpers
 *----------------------------------------------------------*/

static bool filesystem_build_path(
    char *dest,
    size_t size,
    const char *directory,
    const char *name)
{
    if (!dest || !directory || !name)
    {
        return false;
    }

    int written = snprintf(
        dest,
        size,
        "%s/%s",
        directory,
        name);

    return (written >= 0) && ((size_t)written < size);
}

/*----------------------------------------------------------
 * Initialization
 *----------------------------------------------------------*/

void filesystem_init(Filesystem *fs)
{
    if (!fs)
    {
        return;
    }

    memset(fs, 0, sizeof(Filesystem));

    strncpy(
        fs->current_path,
        SD_CARD_MOUNT_POINT,
        sizeof(fs->current_path) - 1);

    fs->mounted = false;
    fs->save_mode = false;
    fs->hex_mode = false;
    fs->file_offset = 0;
}

/*----------------------------------------------------------
 * Mount State
 *----------------------------------------------------------*/

bool filesystem_is_mounted(
    const Filesystem *fs)
{
    if (!fs)
    {
        return false;
    }

    return fs->mounted;
}

void filesystem_set_mounted(
    Filesystem *fs,
    bool mounted)
{
    if (!fs)
    {
        return;
    }

    fs->mounted = mounted;
}

/*----------------------------------------------------------
 * Path Getters
 *----------------------------------------------------------*/

const char *
filesystem_current_path(
    const Filesystem *fs)
{
    if (!fs)
    {
        return "";
    }

    return fs->current_path;
}

const char *
filesystem_active_file(
    const Filesystem *fs)
{
    if (!fs)
    {
        return "";
    }

    return fs->active_file;
}

/*----------------------------------------------------------
 * File Selection
 *----------------------------------------------------------*/

bool filesystem_select_file(
    Filesystem *fs,
    const BrowserEntry *entry)
{
    if (!fs || !entry)
    {
        return false;
    }

    strncpy(
        fs->active_file,
        entry->full_path,
        sizeof(fs->active_file) - 1);

    fs->active_file[sizeof(fs->active_file) - 1] = '\0';

    fs->file_offset = 0;

    ESP_LOGI(
        TAG,
        "Selected file: %s",
        fs->active_file);

    return true;
}

/*----------------------------------------------------------
 * Directory Navigation
 *----------------------------------------------------------*/

bool filesystem_enter_directory(
    Filesystem *fs,
    const char *directory)
{
    if (!fs || !directory)
    {
        return false;
    }

    char new_path[FILE_PATH_LENGTH];

    if (!filesystem_build_path(
            new_path,
            sizeof(new_path),
            fs->current_path,
            directory))
    {
        return false;
    }

    if (strlen(new_path) >= DIRECTORY_PATH_LENGTH)
    {
        ESP_LOGE(TAG, "Path too long: %s", new_path);
        return false;
    }

    strncpy(
        fs->current_path,
        new_path,
        sizeof(fs->current_path) - 1);

    fs->current_path[sizeof(fs->current_path) - 1] = '\0';

    return true;
}

bool filesystem_parent_directory(
    Filesystem *fs)
{
    if (!fs)
    {
        return false;
    }

    /* Never leave the SD card root */
    if (strcmp(fs->current_path, SD_CARD_MOUNT_POINT) == 0)
    {
        return false;
    }

    char *slash = strrchr(fs->current_path, '/');

    if (!slash)
    {
        return false;
    }

    /* Prevent removing "/sdcard" entirely */
    if (slash == fs->current_path)
    {
        return false;
    }

    *slash = '\0';

    return true;
}

/*----------------------------------------------------------
 * Directory Reading
 *----------------------------------------------------------*/

bool filesystem_load_directory(
    Filesystem *fs,
    Browser *browser)
{
    if (!fs || !browser)
    {
        return false;
    }

    browser_clear(browser);

    DIR *dir = opendir(fs->current_path);
    if (!dir)
    {
        ESP_LOGE(TAG, "Failed to open directory: %s", fs->current_path);
        BrowserEntry *entry = browser_add_entry(browser);
        if (entry)
        {
            strcpy(entry->name, "ERR: OPEN FAILED");
            entry->is_directory = false;
        }
        return false;
    }

    /* Add ".." navigation if we are not at the root mount point */
    if (strcmp(fs->current_path, SD_CARD_MOUNT_POINT) != 0 && strcmp(fs->current_path, "/") != 0)
    {
        BrowserEntry *entry = browser_add_entry(browser);
        if (entry)
        {
            strcpy(entry->name, "..");
            entry->is_directory = true;
        }
    }

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL)
    {
        /* Skip current directory reference */
        if (strcmp(ent->d_name, ".") == 0) continue;

        BrowserEntry *entry = browser_add_entry(browser);
        if (!entry) break; /* Reached MAX_DIRECTORY_ENTRIES limit */

        strncpy(entry->name, ent->d_name, sizeof(entry->name) - 1);

        /* Build full path so we can stat() it to accurately determine if it's a directory */
        snprintf(
            entry->full_path, 
            sizeof(entry->full_path), 
            "%s/%s", 
            fs->current_path, 
            ent->d_name);

        struct stat st;
        if (stat(entry->full_path, &st) == 0) 
        {
            entry->is_directory = S_ISDIR(st.st_mode);
        } 
        else 
        {
            /* Fallback if stat fails */
            entry->is_directory = (ent->d_type == DT_DIR);
        }
    }
    
    closedir(dir);
    return true;
}

/*----------------------------------------------------------
 * File I/O
 *----------------------------------------------------------*/

bool filesystem_load_file(
    Filesystem *fs,
    Editor *editor)
{
    if (!fs || !editor)
    {
        return false;
    }

    FILE *f = fopen(fs->active_file, "r");
    if (!f) 
    {
        ESP_LOGE(TAG, "Failed to open file: %s", fs->active_file);
        return false;
    }

    editor_clear(editor);

    if (fseek(f, fs->file_offset, SEEK_SET) != 0)
    {
        ESP_LOGE(TAG, "Failed to seek to offset %u in %s",
            (unsigned)fs->file_offset, fs->active_file);
        fclose(f);
        return false;
    }

    if (fs->hex_mode) 
    {
        uint8_t byte;
        char hex_str[4];
        size_t count = 0;
        
        while (fread(&byte, 1, 1, f) > 0 && count < (EDITOR_BUFFER_SIZE - 4)) 
        {
            snprintf(hex_str, sizeof(hex_str), "%02X ", byte);
            strcat(editor->text, hex_str);
            count += 3;
        }
        
        editor->length = count;
        editor->cursor = 0; // Start cursor at the top of the loaded chunk
    } 
    else 
    {
        size_t bytes_read = fread(editor->text, 1, EDITOR_BUFFER_SIZE - 1, f);
        editor->text[bytes_read] = '\0';
        editor->length = bytes_read;
        editor->cursor = 0; // Start cursor at the top of the loaded chunk
    }

    fclose(f);

    /* Sync the editor's tracking data with the filesystem */
    editor_set_filename(editor, fs->active_file);
    editor_set_offset(editor, fs->file_offset);
    editor_update_status(editor);

    return true;
}

bool filesystem_save_file(
    Filesystem *fs,
    const Editor *editor)
{
    if (!fs || !editor)
    {
        return false;
    }

    /* Fallback to a quicksave if a file hasn't been explicitly opened */
    const char *filepath = fs->active_file;
    if (strlen(filepath) == 0) 
    {
        filepath = SD_CARD_MOUNT_POINT "/quicksave.txt";
    }

    /* Open for update (r+) to seek to a specific page offset, otherwise create (w) */
    FILE *f = fopen(filepath, "r+"); 
    if (!f) 
    {
        f = fopen(filepath, "w"); 
    }

    if (f) 
    {
        if (fseek(f, fs->file_offset, SEEK_SET) != 0)
        {
            ESP_LOGE(TAG, "Failed to seek to offset %u in %s",
                (unsigned)fs->file_offset, filepath);
            fclose(f);
            return false;
        }

        size_t written = fwrite(editor->text, 1, editor->length, f);
        fclose(f);

        if (written != editor->length)
        {
            ESP_LOGE(TAG, "Short write: expected %u, wrote %u to %s",
                (unsigned)editor->length, (unsigned)written, filepath);
            return false;
        }
        
        ESP_LOGI(TAG, "Saved %u bytes to %s at offset %u", 
            (unsigned)editor->length, filepath, (unsigned)fs->file_offset);
        return true;
    }

    ESP_LOGE(TAG, "Failed to save file: %s", filepath);
    return false;
}

/*----------------------------------------------------------
 * Encrypted File I/O Wrappers
 *----------------------------------------------------------*/

bool filesystem_load_encrypted(Filesystem *fs, Editor *editor, const char *password, CryptoMethod method)
{
    if (!fs || !editor || !password) return false;

    // 1. Load the raw file into the editor buffer
    if (!filesystem_load_file(fs, editor)) return false;

    // 2. Calculate required output buffer size
    size_t out_cap = editor->length;
    size_t out_len = 0;
    uint8_t *decrypted = malloc(out_cap);
    if (!decrypted)
    {
        ESP_LOGE(TAG, "Failed to allocate decrypt buffer");
        return false;
    }

    // 3. Decrypt
    if (!crypto_decrypt(
        method,
        (const uint8_t *)editor->text,
        editor->length,
        decrypted,
        &out_len,
        password))
    {
        ESP_LOGE(TAG, "Decryption failed (wrong password or corrupted file?)");
        free(decrypted);
        return false;
    }

    // 4. Copy decrypted data back into editor buffer
    memcpy(editor->text, decrypted, out_len);
    editor->text[out_len] = '\0';
    editor->length = out_len;
    free(decrypted);

    editor_update_status(editor);
    return true;
}

bool filesystem_save_encrypted(Filesystem *fs, Editor *editor, const char *password, CryptoMethod method)
{
    if (!fs || !editor || !password) return false;

    // 1. Calculate output buffer size
    size_t out_cap = crypto_output_size(method, editor->length);
    if (out_cap == 0)
    {
        ESP_LOGE(TAG, "Unknown crypto method");
        return false;
    }

    uint8_t *encrypted = malloc(out_cap);
    if (!encrypted)
    {
        ESP_LOGE(TAG, "Failed to allocate encrypt buffer");
        return false;
    }

    // 2. Encrypt
    size_t enc_len = 0;
    if (!crypto_encrypt(
        method,
        (const uint8_t *)editor->text,
        editor->length,
        encrypted,
        &enc_len,
        password))
    {
        ESP_LOGE(TAG, "Encryption failed");
        free(encrypted);
        return false;
    }

    // 3. Save the encrypted data to disk (using a temp editor copy)
    Editor enc_editor = *editor;
    memcpy(enc_editor.text, encrypted, enc_len);
    enc_editor.text[enc_len] = '\0';
    enc_editor.length = enc_len;
    bool success = filesystem_save_file(fs, &enc_editor);

    free(encrypted);
    return success;
}