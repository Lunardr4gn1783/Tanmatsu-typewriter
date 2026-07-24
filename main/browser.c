#include "browser.h"

#include <string.h>

void browser_init(Browser *browser)
{
    if (!browser)
    {
        return;
    }

    memset(browser, 0, sizeof(Browser));

    browser->visible_items = 10;
}

void browser_clear(Browser *browser)
{
    if (!browser)
    {
        return;
    }

    memset(browser->entries, 0, sizeof(browser->entries));

    browser->count = 0;

    browser_reset(browser);
}

void browser_reset(Browser *browser)
{
    if (!browser)
    {
        return;
    }

    browser->selected = 0;
    browser->scroll = 0;
}

BrowserEntry *
browser_add_entry(Browser *browser)
{
    if (!browser)
    {
        return NULL;
    }

    if (browser->count >= MAX_DIRECTORY_ENTRIES)
    {
        return NULL;
    }

    BrowserEntry *entry =
        &browser->entries[browser->count];

    memset(entry, 0, sizeof(BrowserEntry));

    browser->count++;

    return entry;
}

const BrowserEntry *
browser_selected(const Browser *browser)
{
    if (!browser)
    {
        return NULL;
    }

    if (browser->count == 0)
    {
        return NULL;
    }

    if (browser->selected < 0 || browser->selected >= browser->count)
    {
        return NULL;
    }

    return &browser->entries[browser->selected];
}

bool browser_move_up(Browser *browser)
{
    if (!browser)
    {
        return false;
    }

    if (browser->selected == 0)
    {
        return false;
    }

    browser->selected--;

    if (browser->selected < browser->scroll)
    {
        browser->scroll--;
    }

    return true;
}

bool browser_move_down(Browser *browser)
{
    if (!browser)
    {
        return false;
    }

    if (browser->selected >= (browser->count - 1))
    {
        return false;
    }

    browser->selected++;

    if (browser->selected >= browser->scroll + browser->visible_items)
    {
        browser->scroll++;
    }

    return true;
}

bool browser_select_by_path(Browser *browser, const char *path)
{
    if (!browser || !path || path[0] == '\0')
    {
        return false;
    }

    for (int i = 0; i < browser->count; i++)
    {
        if (strcmp(browser->entries[i].full_path, path) == 0)
        {
            browser->selected = i;

            /* Scroll so the selected item is visible */
            if (browser->selected < browser->scroll)
            {
                browser->scroll = browser->selected;
            }
            else if (browser->selected >= browser->scroll + browser->visible_items)
            {
                browser->scroll = browser->selected - browser->visible_items + 1;
            }

            return true;
        }
    }

    return false;
}