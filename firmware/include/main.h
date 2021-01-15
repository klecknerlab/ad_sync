#if !defined(MAIN_H)

#define MAIN_H 1
#include <Arduino.h>

// I2S config options
#include "driver/i2s.h"

// Is Bluetooth enabled at all?
// #define BLUETOOTH_ENABLED 1

#ifdef BLUETOOTH_ENABLED
    #include "BluetoothSerial.h"
    #if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
    #error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
    #endif
#endif

// Maximum number of serial characters to process per command queue cycle.
// #define MAX_SERIAL_CHAR_PER_CYCLE   16

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
#define I2S_SD_PIN      15
#define OE_PIN          13
#define LED_R_PIN       18
#define LED_G_PIN       19
#define LED_B_PIN       21

const i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_CLK_PIN,
    .ws_io_num = I2S_WS_PIN,
    .data_out_num = I2S_SD_PIN,
    .data_in_num = -1,
};

// LED output gamma -- here matched to typical displays
#define LED_GAMMA       2.2

// The trim is used to scale each output so that max brightness = white
// Output duty cycle is proportional to trim; 65536 is maximum!
// Testing found that with no trim, led 200 255 120 gave white
// R_TRIM = (200/255)^2.2 * 65536
#define LED_R_TRIM      38402 
// R_TRIM = (255/255)^2.2 * 65536
#define LED_G_TRIM      65536
// R_TRIM = (200/255)^2.2 * 65536
#define LED_B_TRIM      12482
const uint32_t LED_TRIM[3] = {LED_R_TRIM, LED_G_TRIM, LED_B_TRIM};
const uint8_t LED_PINS[3] = {LED_R_PIN, LED_G_PIN, LED_B_PIN};

// LED outputs a nice sequence of colors on boot, as defined here
#define LED_STARTUP_LEN 5
// Interval is in ms
#define LED_STARTUP_INTERVAL 750 
const uint8_t LED_STARTUP_SEQ[LED_STARTUP_LEN][3] = {
    {  0,   0,   0},
    {  0,   0, 255},
    {  0, 255, 255},
    {225, 255, 255},
    {  0,   0,   0},
};
extern int startup_colors_active;

// The size of various buffers.
#define SER_BUFFER_SIZE 1024 // Used to buffer all inputs/outputs -- this is in addition to the built in serial buffer, which is 64 bytes.
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

// LED look up table; used for gamma correction
extern uint16_t LED_LUT[256];

void set_led_color(int r, int g, int b);

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
extern unsigned long buffer_update_time, last_sync_update;

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

// Circular buffer class
// This is a non-blocking way of storing serial input/output.
// It will fail silently on overlow, but set the overflow variable.
// Code in "circular_buffer.cpp"
class CircularBuffer {
    public:
        uint8_t buffer[SER_BUFFER_SIZE];
        uint8_t *b_current, *b_end;
        int start, available, overflow;

        CircularBuffer();
        int write(const uint8_t *s);
        int write(const char *s);
        int write(const uint8_t *s, int nbytes);
        int write(const char *s, int nbytes);
        int write(const uint8_t c);
        int from_stream(Stream &stream);
        int to_stream(HardwareSerial &stream);
        #ifdef BLUETOOTH_ENABLED
            int to_stream(BluetoothSerial &stream);
        #endif
        int to_stream(CircularBuffer &buf);
        uint8_t * get_buffer(int * max_data);
        void flush();
};

extern CircularBuffer ser0_output, ser1_input, ser1_output, ser2_input, ser2_output;

#ifdef BLUETOOTH_ENABLED
    extern CircularBuffer serbt_output;
#endif

#endif
