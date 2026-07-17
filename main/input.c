#include "input.h"
#include "app.h"
#include "bsp/input.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static QueueHandle_t input_event_queue = NULL;

bool input_init(void) {
    return (bsp_input_get_queue(&input_event_queue) == ESP_OK);
}

void input_process(void) {
    bsp_input_event_t event;
    
    if (xQueueReceive(input_event_queue, &event, pdMS_TO_TICKS(100)) == pdTRUE) {
        
        if (event.type == INPUT_EVENT_TYPE_KEYBOARD) {
            char key = event.args_keyboard.ascii;
            if (key == '\r') key = '\n';
            
            /* Only route keyboard input in states that accept typing */
            if (app.state == APP_STATE_EDITOR ||
                app.state == APP_STATE_SAVE_AS ||
                app.state == APP_STATE_PASSWORD)
            {
                app_handle_char(key);
            }
            
        } else if (event.type == INPUT_EVENT_TYPE_NAVIGATION && event.args_navigation.state == true) {
            
            app_handle_nav(event.args_navigation.key);
        }
    }
}