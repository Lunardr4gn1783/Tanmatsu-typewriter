#include "input.h"
#include "app.h"
#include "bsp/input.h"
#include "bsp/led.h"
#include "freertos/FreeRTOS.h"
#include "esp_timer.h"

static QueueHandle_t input_event_queue = NULL;

static bool super_held               = false;
static bool shift_toggled_this_press = false;

/* Backspace hold-repeat state */
static bool    backspace_held  = false;
static int64_t backspace_since = 0;
static int64_t backspace_last  = 0;

#define BACKSPACE_REPEAT_DELAY_US    450000  /* hold this long before repeating */
#define BACKSPACE_REPEAT_INTERVAL_US  45000  /* then one delete per interval */

static void set_caps_lock_led(bool on)
{
    if (on) {
        bsp_led_set_mode(false);
        bsp_led_set_pixel_rgb(0, 0, 255, 0);
        bsp_led_set_pixel_rgb(4, 0, 0, 255);
        bsp_led_send();
    } else {
        bsp_led_set_pixel_rgb(0, 0, 0, 0);
        bsp_led_set_pixel_rgb(4, 0, 0, 0);
        bsp_led_send();
        bsp_led_set_mode(true);
    }
}

bool input_init(void) {
    return (bsp_input_get_queue(&input_event_queue) == ESP_OK);
}

void input_process(void) {
    bsp_input_event_t event;

    if (xQueueReceive(input_event_queue, &event, pdMS_TO_TICKS(20)) == pdTRUE) {

        if (event.type == INPUT_EVENT_TYPE_KEYBOARD) {
            char key = event.args_keyboard.ascii;
            uint32_t mods = event.args_keyboard.modifiers;

            /* Ctrl (or Meta) shortcuts in the editor. The BSP may deliver
             * the letter as-is or as a terminal control code, so accept
             * both forms. */
            bool modifier = (mods & (BSP_INPUT_MODIFIER_CTRL |
                                     BSP_INPUT_MODIFIER_SUPER)) || super_held;

            if (modifier && (app.state == APP_STATE_EDITOR ||
                             app.state == APP_STATE_MD_PREVIEW))
            {
                char shortcut = 0;
                switch (key)
                {
                    case 'a': case 'A': case 0x01: shortcut = 'a'; break;
                    case 'c': case 'C': case 0x03: shortcut = 'c'; break;
                    case 'x': case 'X': case 0x18: shortcut = 'x'; break;
                    case 'v': case 'V': case 0x16: shortcut = 'v'; break;
                    case 'm': case 'M': case 0x0D: shortcut = 'm'; break;
                }

                if (shortcut != 0)
                {
                    app_handle_shortcut(shortcut);
                }

                /* Swallow everything typed while a modifier is held so a
                 * Ctrl-combo never inserts a stray character. */
                return;
            }

            if (key == '\r') key = '\n';

            if (app.caps_lock)
            {
                if (key >= 'a' && key <= 'z')
                    key = key - 'a' + 'A';
                else if (key >= 'A' && key <= 'Z')
                    key = key - 'A' + 'a';
            }

            if (app.state == APP_STATE_EDITOR ||
                app.state == APP_STATE_SAVE_AS ||
                app.state == APP_STATE_PASSWORD)
            {
                app_handle_char(key);
            }

        } else if (event.type == INPUT_EVENT_TYPE_NAVIGATION) {

            if (event.args_navigation.key == BSP_INPUT_NAVIGATION_KEY_BACKSPACE)
            {
                backspace_held  = event.args_navigation.state;
                backspace_since = esp_timer_get_time();
                backspace_last  = backspace_since;
            }
            else if (event.args_navigation.state)
                app_handle_nav(event.args_navigation.key,
                               event.args_navigation.modifiers);

        } else if (event.type == INPUT_EVENT_TYPE_SCANCODE) {

            uint16_t raw = event.args_scancode.scancode;
            bool pressed = !(raw & BSP_INPUT_SCANCODE_RELEASE_MODIFIER);
            uint16_t sc  = raw & ~BSP_INPUT_SCANCODE_RELEASE_MODIFIER;

            if (sc == BSP_INPUT_SCANCODE_ESCAPED_LEFTMETA)
            {
                super_held = pressed;
                if (!super_held)
                    shift_toggled_this_press = false;
            }

            if (pressed && super_held &&
                !shift_toggled_this_press &&
                (sc == BSP_INPUT_SCANCODE_LEFTSHIFT ||
                 sc == BSP_INPUT_SCANCODE_RIGHTSHIFT))
            {
                app.caps_lock = !app.caps_lock;
                shift_toggled_this_press = true;
                set_caps_lock_led(app.caps_lock);
            }
        }
    }

    /* Backspace hold-repeat: after an initial delay, keep deleting while
     * the key stays down. The first delete came from the ascii event. */
    if (backspace_held && app.state == APP_STATE_EDITOR)
    {
        int64_t now = esp_timer_get_time();

        if (now - backspace_since > BACKSPACE_REPEAT_DELAY_US &&
            now - backspace_last  > BACKSPACE_REPEAT_INTERVAL_US)
        {
            app_handle_char('\b');
            backspace_last = now;
        }
    }
}
