// This file includes the setup and loop functions, as well as the serial interaction code.
// The actual commands are processed in "commands.cpp", and the sync data is handled in "sync.cpp"

#include "main.h"
#include "commands.h"
#include "sync.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"

// Define some globally shared variables/arrays
uint16_t LED_LUT[256];

// Set up the serial input/output buffers
// Note: these are in *addition* to the Arduino serial buffers.
// This provides a non-blocking method for large reads/writes
CircularBuffer ser0_output, ser1_input, ser1_output, ser2_input, ser2_output;

// Bluetooth setup
#ifdef BLUETOOTH_ENABLED
    char bt_name[BT_NAME_MAX_LENGTH+1];
    BluetoothSerial SerialBT;
    static int bt_enabled = 0;
    CircularBuffer serbt_output;
#endif

TaskHandle_t command_task;         

void setup()
{
    // Build LED lookup table for gamma correction
    for (int i=0; i<256; i++) {
        LED_LUT[i] = int(pow((float)i / 256., LED_GAMMA) * 65535.0 + 0.5);
    }

    // Set up LED outputs, and set to start of startup sequence
    for (int i=0; i<3; i++) {
        ledcSetup(i, 5000, 16);
        ledcAttachPin(LED_PINS[i], i);
        ledcWrite(i, (i==0) ? 255 : 0);
    }

    // Set up OE pin, which enables the shift registers
    pinMode(OE_PIN, OUTPUT);
    digitalWrite(OE_PIN, LOW); // Shift registers enabled

    // Set up serial ports
    Serial.begin(921600); // 0 is used for USB communication with host
    Serial1.begin(9600, SERIAL_8N1, RX1_PIN, TX1_PIN); // Ser1
    Serial2.begin(9600, SERIAL_8N1, RX2_PIN, TX2_PIN); // Ser2
    Serial.flush();
    Serial1.flush();
    Serial2.flush();

    // Set up NVS storage.  At present, this is used only for the bluetooth name
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );

    #ifdef BLUETOOTH_ENABLED
        nvs_handle nvs;
        size_t bt_name_len = BT_NAME_MAX_LENGTH+1;
        err = nvs_open("bluetooth", NVS_READONLY, &nvs);
        if (err != ESP_OK) {
            Serial.write("ERROR: attempt to open NVS handle failed with '");
            Serial.write(esp_err_to_name(err));
            Serial.write("'\n");
        }

        err = nvs_get_str(nvs, "name", bt_name, &bt_name_len);
        switch (err) {
            case ESP_OK:
                if (bt_name[0]) {
                    SerialBT.begin(bt_name);
                    bt_enabled = 1;
                    Serial.write("Bluetooth connection enabled with name: ");
                    Serial.write(bt_name);
                    Serial.write("\n");
                } else {
                    bt_enabled = 0;
                    Serial.write("Bluetooth disabled; use command 'BLUETOOTH' to initialize.");
                }
                break;
            default:
                Serial.write("Error trying to read bluetooth name from NVS (");
                Serial.write(esp_err_to_name(err));
                Serial.write(")\n");
        }
        nvs_close(nvs);
    #endif

    // Init the sync code (see sync.cpp)
    init_sync();
}

esp_err_t bluetooth_set_name(const char* name) {
    esp_err_t err = ESP_ERR_NOT_SUPPORTED;

    #ifdef BLUETOOTH_ENABLED
        nvs_handle nvs;

        err = nvs_open("bluetooth", NVS_READWRITE, &nvs);
        if (err != ESP_OK) {return err;}

        err = nvs_set_str(nvs, "name", bt_name);
        nvs_close(nvs);

        if (err == ESP_OK) {
            SerialBT.end();
            bt_enabled = 0;
            if (bt_name[0]) {
                SerialBT.begin(bt_name);
                bt_enabled = 1;
            }
        }

    #endif
    return err;
}

// Create a static serial function so we can pass it to a command queue.
// void serial_write(const char* s) {Serial.write(s);}
CommandQueue serial_commands;

#ifdef BLUETOOTH_ENABLED
    CommandQueue serial_bt_commands;
#endif

// Note: multiple command queues can operate simultaneously.
// This is designed to allow for future wifi/bluetooth connection modes.
// If these are added, create a NEW command queue with a corresponding output function as the argument.
// You will then need to add a new hook in the loop function below to feed characters to the command processing.

int startup_colors_active = 1;

void loop()
{
    // This loops processes all the command queues, and updates the DMA for the sync
    // Note that update_sync should return quickly if there is nothing to do; it's better
    //  to run it a lot to avoid corrupting the output.

    // Update the LED with pretty colors on boot.
    if (startup_colors_active) {
        unsigned long t = millis();
        int i = t / LED_STARTUP_INTERVAL;
        if (i >= (LED_STARTUP_LEN-1)) {
            i = LED_STARTUP_LEN - 1;
            set_led_color(
                LED_STARTUP_SEQ[i][0],
                LED_STARTUP_SEQ[i][1],
                LED_STARTUP_SEQ[i][2]
            );
            startup_colors_active = 0;
        } else {
            int r1 = LED_STARTUP_SEQ[i  ][0];
            int r2 = LED_STARTUP_SEQ[i+1][0];
            int g1 = LED_STARTUP_SEQ[i  ][1];
            int g2 = LED_STARTUP_SEQ[i+1][1];
            int b1 = LED_STARTUP_SEQ[i  ][2];
            int b2 = LED_STARTUP_SEQ[i+1][2];
            int m2 = (t * 256) / LED_STARTUP_INTERVAL - i * 256;
            int m1 = 256 - m2;
            set_led_color(
                (m1 * r1 + m2 * r2) >> 8,
                (m1 * g1 + m2 * g2) >> 8,
                (m1 * b1 + m2 * b2) >> 8
            );
        }
    }

    // Process input commands from USB
    for (int i=Serial.available(); i>0; i--) {
        int c = Serial.read();
        if (c >= 0) {serial_commands.process_char((char)c);}
        else {break;}
    }
    update_sync();
    serial_commands.output_buffer.to_stream(Serial);
    update_sync();

    // Procuess input commands from Bluetooth
    #ifdef BLUETOOTH_ENABLED
    if (bt_enabled) {
        for (int i=SerialBT.available(); i>0; i--) {
            int c = SerialBT.read();
            if (c >= 0) {serial_bt_commands.process_char((char)c);}
            else {break;}
        }
        update_sync();
        serial_bt_commands.output_buffer.to_stream(SerialBT);
        update_sync();
    }
    #endif

    // Handle aux serial inputs/outputs
    // Should be pretty fast!
    ser1_output.to_stream(Serial1);
    ser2_output.to_stream(Serial2);
    ser1_input.from_stream(Serial1);
    ser2_input.from_stream(Serial2);

    update_sync();
}


void set_led_color(int r, int g, int b) {
    ledcWrite(0, (LED_LUT[max(min(r, 255), 0)] * LED_TRIM[0]) >> 16);
    ledcWrite(1, (LED_LUT[max(min(g, 255), 0)] * LED_TRIM[1]) >> 16);
    ledcWrite(2, (LED_LUT[max(min(b, 255), 0)] * LED_TRIM[2]) >> 16);
}