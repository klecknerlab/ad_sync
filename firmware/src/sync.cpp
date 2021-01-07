#include "main.h"
#include "sync.h"

// Define some global variables.
// These are specified as "extern" in "main.h"
uint last_bytes_written = 0;
uint cycles_since_write = 0;
unsigned long buffer_update_time = 0;
uint16_t ana0_set = 1<<15, ana1_set = 1<<15;
int analog_update = 0;
int analog_sync_mode = 1;
int digital_sync_mode = 0;
uint32_t ana0_multiplier = 1<<16, ana0_offset = 0, ana1_multiplier = 1<<16, ana1_offset = 0;
uint32_t sync_data[SYNC_DATA_SIZE];
int sync_start = 0;
int sync_cycles = 1024;
int sync_active = 0;

// Internal variables
static int sync_end = 1024;
static unsigned long t1;
static uint64_t i2s_write_buffer[I2S_WRITE_BUFFER_SIZE];
static size_t bytes_written = I2S_WRITE_BUFFER_SIZE; // When we've just start up we need to update the output. 
static int sync_i = 0;
static int sync_was_active = 0;


static float APLL_DIV_MIN[NUM_APLL_DIV];

void init_sync() {
    // Install I2S driver; this is used to drive the sync outputs.
    esp_err_t err;

    err = i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    if (err != ESP_OK) {
        Serial.write("Failed installing I2S driver!\n");
        return;
    }

    err = i2s_set_pin(I2S_NUM_0, &pin_config);
    if (err != ESP_OK) {
        Serial.write("Failed setting I2S pins!\n");
        return;
    }

    Serial.write("I2S driver installed succesfully.\n");

    i2s_start(I2S_NUM_0);

    for (int i=0; i<NUM_APLL_DIV; i++) {
        APLL_DIV_MIN[i] = (float)APLL_MIN / (2*(2+APLL_DIV[i][0]) * APLL_DIV[i][1] * APLL_DIV[i][2]);
    }

    sync_freq(102400.0);
}

void update_sync() {
    // Only update the buffer if we need to
    if (bytes_written) { // The buffer was written, so prepare a new one!
        t1 = micros();

        if (sync_active && (!sync_was_active)) {
            sync_i = sync_start;
            sync_end = (sync_start + sync_cycles) % SYNC_DATA_SIZE;
        } 
        sync_was_active = sync_active;

        for (int i=0; i<I2S_WRITE_BUFFER_SIZE; i++) {
            if (sync_active) {
                uint32_t data = sync_data[sync_i];

                // uint32_t ao = (((data & 0xFFFF) * analog_multiplier) >> 16) + analog_offset;
                uint32_t ad = (data & 0xFFFF);
                if (analog_sync_mode == 0) { // Fixed output mode
                    // We need to write something, so just update analog 0
                    ad = (DAC_SPI_CH0 + ana0_set);
                } else { 
                    int channel = 0;

                    // If we're in dual output mode, channel is chosen by address
                    if (analog_sync_mode == 3) {channel = sync_i%2;}
                    // Otherwise its determined by the mode
                    else {channel = analog_sync_mode - 1;}

                    if (channel == 0) {
                        ad = ((ad * ana0_multiplier) >> 16) + ana0_offset + DAC_SPI_CH0;
                    } else {
                        ad = ((ad * ana1_multiplier) >> 16) + ana1_offset + DAC_SPI_CH1;
                    }
                }

                uint32_t dd = (data & 0xFFFF0000) >> 16;
                if (digital_sync_mode == 1) { // OR output mode
                    dd |= (data & 0xFF00) >> 8;
                }

                // Bits 8-31 are analog output -- shift determined by global constant (DAC dependent)
                // Bits 40-63 are digital output.
                i2s_write_buffer[i] = ((uint64_t)(dd) << 40) + (ad<<DAC_SHIFT);

                sync_i ++;
                if (sync_i == sync_end) {
                    sync_i = sync_start;
                    sync_end = (sync_start + sync_cycles) % SYNC_DATA_SIZE;
                }
            } else {
                i2s_write_buffer[i] = (DAC_SPI_CH0 + ana0_set) << DAC_SHIFT;
            }
        }

        // If fixed analog outputs need updating, then update them!
        if ((analog_update & 1) && !(analog_sync_mode & 1)) {
            i2s_write_buffer[0] = (i2s_write_buffer[0] & I2S_DIG_MASK) + ((DAC_SPI_CH0 + ana0_set) << DAC_SHIFT);
        }
        if ((analog_update & 2) && !(analog_sync_mode & 2)) {
            i2s_write_buffer[1] = (i2s_write_buffer[1] & I2S_DIG_MASK) + ((DAC_SPI_CH1 + ana1_set) << DAC_SHIFT);
        }
        analog_update = 0;

        // Collect some stats about the update.
        cycles_since_write = 0;
        last_bytes_written = bytes_written;
        buffer_update_time = micros() - t1;
    } else {
        cycles_since_write++;
    }

    // Try to hand off the buffer to the DMA module.
    // If it's not ready for a new buffer, it will return 0 bytes written, and won't update in the
    //   next pass.
    i2s_write(I2S_NUM_0, i2s_write_buffer, (size_t)(I2S_WRITE_BUFFER_SIZE*8), &bytes_written, 0);
}

float sync_freq(float freq) {
    float clock_freq = min(max(freq, MIN_FREQ), MAX_FREQ) * 2 * I2S_BIT_DEPTH;
    uint32_t odiv=31, N=63, M=63; //Default to minimum frequency case.

    for (int i=0; i<NUM_APLL_DIV; i++) {
        if (clock_freq > APLL_DIV_MIN[i]) {
            odiv = APLL_DIV[i][0];
            N = APLL_DIV[i][1];
            M = APLL_DIV[i][2];
            break;
        }
    }

    float div_ratio = (float)(2 * (odiv + 2) * N * M); // Note: a single precision float has enough accuracy to store this *exactly*
    float mult = clock_freq * div_ratio / (float)APLL_XTAL;
    uint32_t sdm = (uint32_t)((mult - 4) * (float)(1<<16) + 0.5);
    uint8_t sdm2 = (uint8_t)(sdm >> 16);
    uint8_t sdm1 = (uint8_t)((sdm & 0xFF00) >> 8);
    uint8_t sdm0 = (uint8_t)(sdm & 0xFF);
    float actual_clk = (float)APLL_XTAL * (4.0 + (float)sdm2 + (float)sdm1 / 256.0 + (float)sdm0 / 65536.0) / div_ratio;

    #ifdef FREQ_DEBUG
        Serial.print("Target I2S freq: ");
        Serial.print(clock_freq, 3);
        Serial.print("\nsdm: ");
        Serial.print(sdm);
        Serial.print("\nsdm2: ");
        Serial.print(sdm2);
        Serial.print("\nsdm1: ");
        Serial.print(sdm1);
        Serial.print("\nsdm0: ");
        Serial.print(sdm0);        
        Serial.print("\nodiv: ");
        Serial.print(odiv);
        Serial.print("\nN: ");
        Serial.print(N);
        Serial.print("\nM: ");
        Serial.print(M);
        Serial.print("\nActual I2S freq: ");
        Serial.print(actual_clk, 3);
        Serial.print("\n");
    #endif

    // Use ESP32 function to change APLL
    rtc_clk_apll_enable(1, sdm0, sdm1, sdm2, odiv);

    // The other divisors require direct register modification

    // I2S_SAMPLE_RATE_CONF_REG
    //   bits=24 -> I2S_[TX/RX]_BITS_MOD -> [23:18], [17:12]
    //   M -> I2S_[TX/RX]_BCK_DIV_NUM[5:0] -> [11:6], [5:0]
    WRITE_PERI_REG(I2S_SAMPLE_RATE_CONF_REG(0), (I2S_BIT_DEPTH<<18) + (I2S_BIT_DEPTH<<12) + (M<<6) + M);
    
    // I2S_CLKM_CONF_REG
    //   1 -> I2S_CLKA_ENA -> [21]
    //   N -> REG_CLKM_DIV_NUM[7:0] -> [7:0]
    //   a=1 -> I2S_CLKM_DIV_A[5:0] -> [19:14]
    //   b=0 -> i2S_CLKM-DIV_B[5:0] -> [13:8]
    WRITE_PERI_REG(I2S_CLKM_CONF_REG(0), (1<<21) + (1<<14) + N);

    return actual_clk / 48;
}

