#ifndef INPUT_H
#define INPUT_H

#include <stdbool.h>

// Initialize the BSP input queue
bool input_init(void);

// Non-blocking function to process the FreeRTOS event queue
// and pass parsed keys to the active application state.
void input_process(void);

#endif