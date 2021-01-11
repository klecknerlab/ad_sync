#if !defined(MAIN_H)

#define MAIN_H 1
#include <Arduino.h>


// Is Bluetooth enabled at all?
// #define BLUETOOTH_ENABLED 1

#ifdef BLUETOOTH_ENABLED
    #if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
    #error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
    #endif
#endif


// If defined, print debugging messages.  Not recommended for general use!
// #define CMD_DEBUG 1
// #define FREQ_DEBUG 1

// Version numbers.
#define VERSION_MAJOR 1
#define VERSION_MINOR 0


// The output GPIO pin for a variety of functions
#define RX1_PIN         34
#define TX1_PIN         32
#define RX2_PIN         35
#define TX2_PIN         33
#define I2S_CLK_PIN     16
#define I2S_WS_PIN      17
#define I2S_SD_PIN      4
#define OE_PIN          13
#define LED_R_PIN       21
#define LED_G_PIN       19
#define LED_B_PIN       18

// LED output gamma -- here matched to typical displays
#define LED_GAMMA       2.2

// The trim is used to scale each output so that max brightness = white
// Output duty cycle is proportional to trim; 65536 is maximum!
#define LED_R_TRIM      65536
#define LED_G_TRIM      65536
#define LED_B_TRIM      65536
const uint32_t LED_TRIM[3] = {LED_R_TRIM, LED_G_TRIM, LED_B_TRIM};
const uint8_t LED_PINS[3] = {LED_R_PIN, LED_G_PIN, LED_B_PIN};


// The size of various buffers.
#define SER_BUFFER_SIZE 1024 // Used to buffer Serial1/2 -- this is in addition to the built in serial buffer, which is 64 bytes.
#define SYNC_DATA_SIZE  16384 // Sync data storage.  Larger sizes seem to result in memory errors.
#define I2S_WRITE_BUFFER_SIZE 64 // Used to compute output values


// General purpose macros
#define min(a,b) (((a)<(b))?(a):(b))
#define max(a,b) (((a)>(b))?(a):(b))

// Bit depth of I2S output 
// Note: if you need to change this, it will require *many* alterations to other parts of the code!
// (This is just defined for convenience -- don't change it!)
#define I2S_BIT_DEPTH 24

// Globally shared variables and arrays
// Most of these are output settings which need to be modified by the command queue

// Serial buffers and counts
extern char ser1_buffer[SER_BUFFER_SIZE+1];
extern int ser1_buffer_count;
extern char ser2_buffer[SER_BUFFER_SIZE+1];
extern int ser2_buffer_count;

// LED look up table; used for gamma correction
extern uint16_t LED_LUT[256];

// I2S buffer used to write new samples
// extern uint64_t i2s_write_buffer[I2S_WRITE_BUFFER_SIZE]; // Does not need to be global

// Data for the sync outputs
extern uint32_t sync_data[SYNC_DATA_SIZE];

// The start and length of the current cycle
extern int sync_start, sync_cycles;

// Is the sync output active?
extern int sync_active;

// Minimum and maximum frequency, set by the limits of the APLL clock
#define MIN_FREQ 30
#define MAX_FREQ 700000

// Used to compute "sync stat" output
extern uint last_bytes_written, cycles_since_write;
extern unsigned long buffer_update_time;

// Analog default outputs when not in sync mode
extern uint16_t ana0_set, ana1_set;

// Analog channel flags; bit 0 => ana0, bit 1 -> ana1
// If update is set for a channel, then the "default" value needs to be sent to the DAC
extern int analog_update, analog_sync_mode;

// Used to scale analog output
extern uint32_t ana0_multiplier, ana0_offset, ana1_multiplier, ana1_offset;

// Digital output mode
extern int digital_sync_mode;

// Trigger count
extern int trigger_count;
extern uint32_t trigger_mask;

// Function to change frequency
float sync_freq(float freq);

// Maximum string length for bluetooth serial, to avoid infinite writes
#ifdef BLUETOOTH_ENABLED
    #define SERIAL_BT_MAX_WRITE 1024
    #define BT_NAME_MAX_LENGTH 256
    extern char bt_name[BT_NAME_MAX_LENGTH+1];
#endif
esp_err_t bluetooth_set_name(const char* name);

#endif
