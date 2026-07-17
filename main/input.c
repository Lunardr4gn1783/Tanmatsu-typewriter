#include "input.h"
#include "app.h"
#include "bsp/input.h"
#include "bsp/led.h"
#include "freertos/FreeRTOS.h"

static QueueHandle_t input_event_queue = NULL;

static bool super_held               = false;
static bool shift_toggled_this_press = false;

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

    if (xQueueReceive(input_event_queue, &event, pdMS_TO_TICKS(100)) == pdTRUE) {

        if (event.type == INPUT_EVENT_TYPE_KEYBOARD) {
            char key = event.args_keyboard.ascii;
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

            if (event.args_navigation.state)
                app_handle_nav(event.args_navigation.key);

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
}
