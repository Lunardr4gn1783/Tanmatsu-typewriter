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
    
    // pdMS_TO_TICKS(100) allows the loop to breathe for the cursor blink timer
    if (xQueueReceive(input_event_queue, &event, pdMS_TO_TICKS(100)) == pdTRUE) {
        
        // Translate BSP events into your app logic
        if (event.type == INPUT_EVENT_TYPE_KEYBOARD) {
            char key = event.args_keyboard.ascii;
            if (key == '\r') key = '\n';
            
            // Route to the active application state
            app_handle_char(key); 
            
        } else if (event.type == INPUT_EVENT_TYPE_NAVIGATION && event.args_navigation.state == true) {
            
            // Route special keys
            app_handle_nav(event.args_navigation.key);
        }
    }
}