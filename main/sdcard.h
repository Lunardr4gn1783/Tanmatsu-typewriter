#ifndef SDCARD_H
#define SDCARD_H

#include <stdbool.h>

/* Initializes the LDO power control, power cycles the slot, 
 * and mounts the FAT filesystem to SD_CARD_MOUNT_POINT. */
bool sdcard_init(void);

/* Safely unmounts and disables power to the SD card. */
void sdcard_deinit(void);

#endif