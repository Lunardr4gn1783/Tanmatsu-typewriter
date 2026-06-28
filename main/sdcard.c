#include "sdcard.h"
#include "config.h" // Grabs SD_CARD_MOUNT_POINT

#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "hal/ldo_types.h"
#include "sd_pwr_ctrl.h"
#include "sd_pwr_ctrl_by_on_chip_ldo.h"
#include "sdmmc_cmd.h"
#include "sd_protocol_types.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "sdcard";

/*------------------------------------------------------------------
 * Tanmatsu Pin Definitions
 * (If you haven't moved these to a global hardware header, 
 * they live perfectly safe here)
 *----------------------------------------------------------------*/
#define BSP_SDCARD_CLK  GPIO_NUM_43
#define BSP_SDCARD_CMD  GPIO_NUM_44
#define BSP_SDCARD_D0   GPIO_NUM_39
#define BSP_SDCARD_D1   GPIO_NUM_40
#define BSP_SDCARD_D2   GPIO_NUM_41
#define BSP_SDCARD_D3   GPIO_NUM_42

/* Keep track of the card handle for safe unmounting later */
static sdmmc_card_t *mount_card = NULL;

/*------------------------------------------------------------------
 * Initialization
 *----------------------------------------------------------------*/
bool sdcard_init(void) 
{
    esp_err_t ret;

    ESP_LOGI(TAG, "Initializing Tanmatsu SD Card Power Control...");
    
    sd_pwr_ctrl_ldo_config_t ldo_config = { .ldo_chan_id = LDO_UNIT_4 };
    sd_pwr_ctrl_handle_t pwr_ctrl_handle = NULL;

    ret = sd_pwr_ctrl_new_on_chip_ldo(&ldo_config, &pwr_ctrl_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create LDO power control: %s", esp_err_to_name(ret));
        return false;
    }

    ESP_LOGI(TAG, "Power cycling SD card...");
    sd_pwr_ctrl_set_io_voltage(pwr_ctrl_handle, 0);     
    vTaskDelay(pdMS_TO_TICKS(150));                     
    sd_pwr_ctrl_set_io_voltage(pwr_ctrl_handle, 3300);   
    vTaskDelay(pdMS_TO_TICKS(150));                     

    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.clk    = BSP_SDCARD_CLK;  
    slot_config.cmd    = BSP_SDCARD_CMD;  
    slot_config.d0     = BSP_SDCARD_D0;   
    slot_config.d1     = BSP_SDCARD_D1;   
    slot_config.d2     = BSP_SDCARD_D2;   
    slot_config.d3     = BSP_SDCARD_D3;   
    slot_config.width  = 4;

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.slot = SDMMC_HOST_SLOT_0;  
    host.pwr_ctrl_handle = pwr_ctrl_handle;
    host.max_freq_khz = SDMMC_FREQ_HIGHSPEED; 
    
    // Allocate DMA-aligned buffer for the host
    static DRAM_DMA_ALIGNED_ATTR uint8_t dma_buf[512 * 4]; 
    host.dma_aligned_buffer = dma_buf;

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed   = false,
        .max_files                = MAX_OPEN_FILES, // Pulling from your config.h
        .allocation_unit_size     = ALLOCATION_UNIT_SIZE, // Pulling from your config.h
        .disk_status_check_enable = false,
        .use_one_fat              = false,
    };

    ESP_LOGI(TAG, "Mounting FAT filesystem to %s...", SD_CARD_MOUNT_POINT);
    
    ret = esp_vfs_fat_sdmmc_mount(SD_CARD_MOUNT_POINT, &host, &slot_config, &mount_config, &mount_card);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SD card: %s", esp_err_to_name(ret));
        return false;
    } 
    
    ESP_LOGI(TAG, "SD card mounted successfully!");
    return true;
}

/*------------------------------------------------------------------
 * Cleanup
 *----------------------------------------------------------------*/
void sdcard_deinit(void) 
{
    if (mount_card != NULL) {
        ESP_LOGI(TAG, "Unmounting SD card...");
        
        esp_err_t ret = esp_vfs_fat_sdcard_unmount(SD_CARD_MOUNT_POINT, mount_card);
        
        if (ret == ESP_OK) {
            mount_card = NULL;
            ESP_LOGI(TAG, "SD card unmounted successfully.");
        } else {
            ESP_LOGE(TAG, "Failed to unmount SD card: %s", esp_err_to_name(ret));
        }
    }
}