#include "browser.h"

#include <string.h>

void browser_init(Browser *browser)
{
    if (!browser)
    {
        return;
    }

    memset(browser, 0, sizeof(Browser));
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

    return true;
}