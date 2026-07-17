#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "nvs_flash.h"
#include "bsp/device.h"
#include "esp_log.h"

#include "app.h"
#include "sdcard.h"
#include "renderer.h"
#include "input.h"

static const char *TAG = "main";

void app_main(void) 
{
    // 1. Core Hardware & Memory Init
    gpio_install_isr_service(0);
    nvs_flash_init();

    // 2. Display Init
    const bsp_configuration_t bsp_config = {
        .display = {.requested_color_format = BSP_DISPLAY_COLOR_FORMAT_24_888RGB, .num_fbs = 1}
    };
    bsp_device_initialize(&bsp_config);

    // 3. Tanmatsu Subsystems Init
    if (!sdcard_init()) {
        ESP_LOGE(TAG, "SD card init failed");
    }
    if (!renderer_init()) {
        ESP_LOGE(TAG, "Renderer init failed");
    }
    if (!input_init()) {
        ESP_LOGE(TAG, "Input init failed");
    }
    
    // 4. Application Logic Init
    app_init();

    // 5. Main Task Loop
    while (1) 
    {
        // Drain the queue and update state flags
        input_process();

        // If a state change requested a redraw, push the frame
        if (app.redraw_request != REDRAW_NONE) 
        {
            renderer_render(&app);
        }
    }
}