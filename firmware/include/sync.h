#if !defined(SYNC_H)

#define SYNC_H 1
#include <Arduino.h>

// The shared sync variables are defined in main.h, since they are also accessed by command.h

// Function to update outputs
void update_sync();

#endif