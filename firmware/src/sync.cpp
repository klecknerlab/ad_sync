#include "main.h"
#include "sync.h"

// Define some global variables.
// These are specified as "extern" in "main.h"
uint bytes_written = I2S_WRITE_BUFFER_SIZE; // When we've just start up we need to update the output. 
uint last_bytes_written = 0;
uint cycles_since_write = 0;
unsigned long buffer_update_time = 0;
uint16_t analog_default[2] = {0, 0};
int analog_update = 0;
int analog_enabled = 1;
uint analog_multiplier = 1<<16;
uint analog_offset = 10;
uint32_t sync_data[SYNC_DATA_SIZE];
int sync_start = 0;
int sync_cycles = 1024;

// Internal variables
static int sync_end = 1024;
static int sync_i = 0;
static unsigned long t1;
static uint64_t i2s_write_buffer[I2S_WRITE_BUFFER_SIZE];
static int has_buffer = 0;

void update_sync() {
    // Only update the buffer if we need to
    if (bytes_written) { // The buffer was written, so prepare a new one!
        t1 = micros();

        for (int i=0; i<I2S_WRITE_BUFFER_SIZE; i++) {
            uint32_t data = sync_data[sync_i];

            uint32_t ao = (((data & 0xFFFF) * analog_multiplier) >> 16) + analog_offset;

            // Bits 16-32 are analog output (MCP4822)
            // Here we are dropping the bottom 4 bits of the analog signal, since
            //   the DAC is only 12 bits.
            // Bits 40-63 are digital output.
            i2s_write_buffer[i] = ((uint64_t)(data & 0xFFFF0000) << 24) + (0b11<<28) + (ao<<12);

            sync_i ++;
            if (sync_i == sync_end) {
                sync_i = sync_start;
                sync_end = (sync_start + sync_cycles) % SYNC_DATA_SIZE;
            }
        }

        cycles_since_write = 0;
        last_bytes_written = bytes_written;

        buffer_update_time = micros() - t1;
        has_buffer = 1;
    } else {
        cycles_since_write++;
    }

    // Try to hand off the buffer to the DMA module.
    // If it's not ready for a new buffer, it will return 0 bytes written, and won't update in the
    //   next pass.
    i2s_write(I2S_NUM_0, i2s_write_buffer, (size_t)(I2S_WRITE_BUFFER_SIZE*8), &bytes_written, 0);
}

