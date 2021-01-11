// This file includes the setup and loop functions, as well as the serial interaction code.
// The actual commands are processed in "commands.cpp", and the sync data is handled in "sync.cpp"

#include "main.h"
#include "commands.h"
#include "sync.h"
#ifdef BLUETOOTH_ENABLED
    #include "BluetoothSerial.h"
#endif
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"

// Define some globally shared variables/arrays
char ser1_buffer[SER_BUFFER_SIZE+1];
int ser1_buffer_count = 0;
char ser2_buffer[SER_BUFFER_SIZE+1];
int ser2_buffer_count = 0;
uint16_t LED_LUT[256];

// Bluetooth setup
#ifdef BLUETOOTH_ENABLED
    char bt_name[BT_NAME_MAX_LENGTH+1];
    BluetoothSerial SerialBT;
    static int bt_enabled = 0;
#endif



void setup()
{
    // Build LED lookup table for gamma correction
    for (int i=0; i<256; i++) {
        LED_LUT[i] = int(pow((float)i / 256., LED_GAMMA) * 65535.0 + 0.5);
    }

    // Set up LED outputs, and set to off
    for (int i=0; i<3; i++) {
        ledcSetup(i, 5000, 16);
        ledcAttachPin(LED_PINS[i], i);
        ledcWrite(i, 0);
    }

    // Set up OE pin, which enables the shift registers
    pinMode(OE_PIN, OUTPUT);
    digitalWrite(OE_PIN, LOW); // Shift registers enabled

    // Set up serial ports
    Serial.begin(115200); // 0 is used for USB communication with host
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

    // Create some example sync data.
    for (int i=0; i<1024; i++) {
        sync_data[i] = ((uint32_t)i<<16) + (i<<6);
    }
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
void serial_write(const char* s) {Serial.write(s);}
CommandQueue serial_commands(serial_write);

#ifdef BLUETOOTH_ENABLED
    // Ditto for bluetooth
    void serial_bt_write(const char* s) {
        char c;
        for (int i=0; i<SERIAL_BT_MAX_WRITE; i++) {
            c = s[i];
            if (c == 0) {
                break;
            } else {
                SerialBT.write(c);
            }
        }
    }
    CommandQueue serial_bt_commands(serial_bt_write);
#endif

// Note: multiple command queues can operate simultaneously.
// This is designed to allow for future wifi/bluetooth connection modes.
// If these are added, create a NEW command queue with a corresponding output function as the argument.
// You will then need to add a new hook in the loop function below to feed characters to the command processing.

void loop()
{
    int n;

    // Process incoming commands.  Reply is automatic when a command is complete. 
    // (Output is handled through the function passed on the creation of the CommandQueue object.)
    for (int i=Serial.available(); i>0; i--) {
        int c = Serial.read();
        if (c >= 0) {serial_commands.process_char((char)c);}
        else {break;}
        // Serial.write((char)c); //echo for debugging
    }

    #ifdef BLUETOOTH_ENABLED
    if (bt_enabled) {
        for (int i=SerialBT.available(); i>0; i--) {
            int c = SerialBT.read();
            if (c >= 0) {serial_bt_commands.process_char((char)c);}
            else {break;}
            // Serial.write((char)c); //echo for debugging
        }
    }
    #endif

    // Check if there is anything in the serial buffers, and update as needed.
    // Note: here we are moving data from the Arduino buffer (64 chars) to a larger secondary buffer.
    n = min(Serial1.available(), SER_BUFFER_SIZE - ser1_buffer_count);
    if (n) {ser1_buffer_count += Serial1.readBytes(ser1_buffer + ser1_buffer_count, n);}

    n = min(Serial2.available(), SER_BUFFER_SIZE - ser2_buffer_count);
    if (n) {ser2_buffer_count += Serial2.readBytes(ser2_buffer + ser2_buffer_count, n);}

    // Update the sync outputs.
    // All settings are controlled through global variables.
    update_sync();
}
