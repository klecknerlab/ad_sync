#if !defined(SYNC_H)

#define SYNC_H 1
#include <Arduino.h>

// The shared sync variables are defined in main.h, since they are also accessed by command.h

// Constants used only by sync.cpp

// These are the SPI header bits used for the MCP4822
// Note: the data is always 16 bits, so we are shifting to the left of that.
#define DAC_SPI_CH0 (0b0011 << 16)
#define DAC_SPI_CH1 (0b1011 << 16)
// Bit shift required to align the data.  The buffer is 32 bits, and the data+header is 20 bits
#define DAC_SHIFT 12

// Output masks for analog and digital data
#define I2S_DIG_MASK (0xFFFFFFFF00000000)
#define I2S_ANA_MASK (0x00000000FFFFFFFF)

#define APLL_MIN 350000000
#define APLL_MAX 560000000
#define APLL_XTAL 40000000
#define NUM_APLL_DIV 36

// Used to get rtc_clk_apll_enable
#include <soc/rtc.h>


const i2s_config_t i2s_config = {
    .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = 10000, // This is irrelevant, changed later by other code
    .bits_per_sample = I2S_BITS_PER_SAMPLE_24BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_LSB), // "LSB" alignment is really MSB alignment.  Don't ask me why!
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = (2*I2S_WRITE_BUFFER_SIZE),
    .use_apll = true,
};


static const uint8_t APLL_DIV[NUM_APLL_DIV][3] = {
    { 0,  2,  2}, { 1,  2,  2}, { 2,  2,  2}, { 4,  2,  2}, { 6,  2,  2}, 
    { 5,  3,  2}, {12,  2,  2}, { 3,  5,  3}, { 5,  7,  2}, {30,  2,  2}, 
    {26,  3,  2}, {20,  5,  2}, {30,  3,  3}, {23,  5,  3}, { 2, 61,  2}, 
    { 4, 53,  2}, {21, 18,  2}, {20,  7,  7}, {25, 26,  2}, {27, 21,  3}, 
    {31, 36,  2}, {24, 17,  7}, {23, 23,  7}, {26, 17, 11}, {21, 37,  8}, 
    {21, 55,  7}, { 5, 47, 35}, {29, 23, 21}, {31, 59, 10}, {29, 43, 19}, 
    {30, 49, 21}, {25, 61, 26}, {30, 60, 29}, {29, 57, 41}, {27, 58, 56}, 
    {31, 63, 63}
};

// Function to initialize outputs
void init_sync();

// Function to update outputs
void update_sync();

// Function to change frequency
// This is defined in "main.h" instead, as it's used by commands.cpp
// float sync_freq(float freq);

#endif