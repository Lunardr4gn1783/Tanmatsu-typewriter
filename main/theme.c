#include "theme.h"

#include <stddef.h>

static const Theme themes[THEME_COUNT] = {

    /* 0: Dark — the original look */
    {
        .name      = "Dark",
        .bg        = 0xFF000000,
        .fg        = 0xFFFFFFFF,
        .dim       = 0xFF888888,
        .accent    = 0xFF0055FF,
        .highlight = 0xFF0055FF,
        .header_bg = 0xFF222222,
        .status_bg = 0xFFFF0000,
        .status_fg = 0xFFFFFFFF,
        .row1      = 0xFF111111,
        .row2      = 0xFF1A1A1A,
    },

    /* 1: Paper — black on warm white */
    {
        .name      = "Paper",
        .bg        = 0xFFF5F0E6,
        .fg        = 0xFF1A1A1A,
        .dim       = 0xFF777777,
        .accent    = 0xFF0044AA,
        .highlight = 0xFFC8D8F0,
        .header_bg = 0xFFE0DACC,
        .status_bg = 0xFF333333,
        .status_fg = 0xFFF5F0E6,
        .row1      = 0xFFEFE9DC,
        .row2      = 0xFFE6E0D2,
    },

    /* 2: Amber — vintage terminal */
    {
        .name      = "Amber",
        .bg        = 0xFF100800,
        .fg        = 0xFFFFB000,
        .dim       = 0xFF805800,
        .accent    = 0xFFFFD060,
        .highlight = 0xFF503000,
        .header_bg = 0xFF201200,
        .status_bg = 0xFF804000,
        .status_fg = 0xFFFFE0A0,
        .row1      = 0xFF180C00,
        .row2      = 0xFF221200,
    },

    /* 3: Green — classic phosphor */
    {
        .name      = "Green",
        .bg        = 0xFF000800,
        .fg        = 0xFF33FF33,
        .dim       = 0xFF1A8020,
        .accent    = 0xFF80FF80,
        .highlight = 0xFF004400,
        .header_bg = 0xFF001800,
        .status_bg = 0xFF006600,
        .status_fg = 0xFFCCFFCC,
        .row1      = 0xFF001000,
        .row2      = 0xFF001800,
    },

    /* 4: Solarized Dark */
    {
        .name      = "Solarized Dark",
        .bg        = 0xFF002B36,
        .fg        = 0xFF93A1A1,
        .dim       = 0xFF586E75,
        .accent    = 0xFF268BD2,
        .highlight = 0xFF073642,
        .header_bg = 0xFF00252E,
        .status_bg = 0xFFB58900,
        .status_fg = 0xFF002B36,
        .row1      = 0xFF002F3B,
        .row2      = 0xFF073642,
    },

    /* 5: Solarized Light */
    {
        .name      = "Solarized Light",
        .bg        = 0xFFFDF6E3,
        .fg        = 0xFF586E75,
        .dim       = 0xFF93A1A1,
        .accent    = 0xFFCB4B16,
        .highlight = 0xFFE4DDC6,
        .header_bg = 0xFFEEE8D5,
        .status_bg = 0xFF586E75,
        .status_fg = 0xFFFDF6E3,
        .row1      = 0xFFF7F0DD,
        .row2      = 0xFFEEE8D5,
    },

    /* 6: Nord */
    {
        .name      = "Nord",
        .bg        = 0xFF2E3440,
        .fg        = 0xFFD8DEE9,
        .dim       = 0xFF616E88,
        .accent    = 0xFF88C0D0,
        .highlight = 0xFF434C5E,
        .header_bg = 0xFF3B4252,
        .status_bg = 0xFF5E81AC,
        .status_fg = 0xFFECEFF4,
        .row1      = 0xFF333947,
        .row2      = 0xFF3B4252,
    },

    /* 7: Dracula */
    {
        .name      = "Dracula",
        .bg        = 0xFF282A36,
        .fg        = 0xFFF8F8F2,
        .dim       = 0xFF6272A4,
        .accent    = 0xFFBD93F9,
        .highlight = 0xFF44475A,
        .header_bg = 0xFF21222C,
        .status_bg = 0xFFFF79C6,
        .status_fg = 0xFF282A36,
        .row1      = 0xFF2C2E3A,
        .row2      = 0xFF313342,
    },

    /* 8: Gruvbox */
    {
        .name      = "Gruvbox",
        .bg        = 0xFF282828,
        .fg        = 0xFFEBDBB2,
        .dim       = 0xFF928374,
        .accent    = 0xFFFE8019,
        .highlight = 0xFF504945,
        .header_bg = 0xFF3C3836,
        .status_bg = 0xFFD79921,
        .status_fg = 0xFF282828,
        .row1      = 0xFF2D2B27,
        .row2      = 0xFF353330,
    },

    /* 9: Monokai */
    {
        .name      = "Monokai",
        .bg        = 0xFF272822,
        .fg        = 0xFFF8F8F2,
        .dim       = 0xFF75715E,
        .accent    = 0xFFA6E22E,
        .highlight = 0xFF49483E,
        .header_bg = 0xFF3E3D32,
        .status_bg = 0xFFF92672,
        .status_fg = 0xFFF8F8F2,
        .row1      = 0xFF2D2E27,
        .row2      = 0xFF34352D,
    },

    /* 10: Ice — cool blue on near-black */
    {
        .name      = "Ice",
        .bg        = 0xFF0A1220,
        .fg        = 0xFFCFE8FF,
        .dim       = 0xFF4A6A8A,
        .accent    = 0xFF66CCFF,
        .highlight = 0xFF16324A,
        .header_bg = 0xFF101E30,
        .status_bg = 0xFF2266AA,
        .status_fg = 0xFFE0F2FF,
        .row1      = 0xFF0D1826,
        .row2      = 0xFF122030,
    },

    /* 11: Midnight — deep blue */
    {
        .name      = "Midnight",
        .bg        = 0xFF000028,
        .fg        = 0xFFC8C8FF,
        .dim       = 0xFF5050A0,
        .accent    = 0xFF8080FF,
        .highlight = 0xFF202060,
        .header_bg = 0xFF000038,
        .status_bg = 0xFF4040A0,
        .status_fg = 0xFFE0E0FF,
        .row1      = 0xFF040430,
        .row2      = 0xFF080840,
    },

    /* 12: Rose — warm pink */
    {
        .name      = "Rose",
        .bg        = 0xFF1A0A12,
        .fg        = 0xFFFF9EC4,
        .dim       = 0xFF8A4A66,
        .accent    = 0xFFFF5CA0,
        .highlight = 0xFF4A1830,
        .header_bg = 0xFF260E1A,
        .status_bg = 0xFFC43C78,
        .status_fg = 0xFFFFE0EE,
        .row1      = 0xFF200C16,
        .row2      = 0xFF2A101E,
    },

    /* 13: Cyan — bright terminal cyan */
    {
        .name      = "Cyan",
        .bg        = 0xFF001010,
        .fg        = 0xFF00E8E8,
        .dim       = 0xFF007070,
        .accent    = 0xFF80FFFF,
        .highlight = 0xFF004848,
        .header_bg = 0xFF001C1C,
        .status_bg = 0xFF008888,
        .status_fg = 0xFFCCFFFF,
        .row1      = 0xFF001414,
        .row2      = 0xFF001C1C,
    },

    /* 14: Crimson — red on near-black */
    {
        .name      = "Crimson",
        .bg        = 0xFF140000,
        .fg        = 0xFFFF6050,
        .dim       = 0xFF803028,
        .accent    = 0xFFFFA090,
        .highlight = 0xFF481010,
        .header_bg = 0xFF200404,
        .status_bg = 0xFF902020,
        .status_fg = 0xFFFFD0C8,
        .row1      = 0xFF180202,
        .row2      = 0xFF220606,
    },

    /* 15: Sepia — old book */
    {
        .name      = "Sepia",
        .bg        = 0xFFE8DCC0,
        .fg        = 0xFF3A2A14,
        .dim       = 0xFF8A7A5A,
        .accent    = 0xFF8A4A10,
        .highlight = 0xFFC8B890,
        .header_bg = 0xFFD8CCA8,
        .status_bg = 0xFF5A452A,
        .status_fg = 0xFFF0E8D4,
        .row1      = 0xFFE2D6B8,
        .row2      = 0xFFDACDAC,
    },

    /* 16: Contrast — maximum readability */
    {
        .name      = "Contrast",
        .bg        = 0xFF000000,
        .fg        = 0xFFFFFFFF,
        .dim       = 0xFFAAAAAA,
        .accent    = 0xFFFFFF00,
        .highlight = 0xFF0000EE,
        .header_bg = 0xFF202020,
        .status_bg = 0xFFFFFF00,
        .status_fg = 0xFF000000,
        .row1      = 0xFF000000,
        .row2      = 0xFF141414,
    },

    /* 17: Ocean — teal */
    {
        .name      = "Ocean",
        .bg        = 0xFF00181E,
        .fg        = 0xFFB8E8E0,
        .dim       = 0xFF4A8078,
        .accent    = 0xFF30C8B0,
        .highlight = 0xFF084A44,
        .header_bg = 0xFF042228,
        .status_bg = 0xFF0A6E62,
        .status_fg = 0xFFDFFFF8,
        .row1      = 0xFF021C22,
        .row2      = 0xFF06262C,
    },

    /* 18: Violet — purple */
    {
        .name      = "Violet",
        .bg        = 0xFF12081E,
        .fg        = 0xFFD8B8FF,
        .dim       = 0xFF7A5AA0,
        .accent    = 0xFFA060FF,
        .highlight = 0xFF341A58,
        .header_bg = 0xFF1C0E2E,
        .status_bg = 0xFF6A30B0,
        .status_fg = 0xFFF0E4FF,
        .row1      = 0xFF160A24,
        .row2      = 0xFF1E1030,
    },

    /* 19: Slate — neutral grayscale */
    {
        .name      = "Slate",
        .bg        = 0xFF181818,
        .fg        = 0xFFD0D0D0,
        .dim       = 0xFF808080,
        .accent    = 0xFFFFFFFF,
        .highlight = 0xFF3A3A3A,
        .header_bg = 0xFF242424,
        .status_bg = 0xFF505050,
        .status_fg = 0xFFF0F0F0,
        .row1      = 0xFF1C1C1C,
        .row2      = 0xFF222222,
    },
};

const Theme *theme_get(int index)
{
    if (index < 0 || index >= THEME_COUNT)
    {
        index = 0;
    }
    return &themes[index];
}

const char *theme_name(int index)
{
    return theme_get(index)->name;
}
