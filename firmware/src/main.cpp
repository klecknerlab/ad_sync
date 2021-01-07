// This file includes the setup and loop functions, as well as the serial interaction code.
// The actual commands are processed in "commands.cpp", and the sync data is handled in "sync.cpp"

#include "main.h"
#include "commands.h"
#include "sync.h"

// Define some globally shared variables/arrays
char ser1_buffer[SER_BUFFER_SIZE+1];
int ser1_buffer_count = 0;
char ser2_buffer[SER_BUFFER_SIZE+1];
int ser2_buffer_count = 0;
uint16_t LED_LUT[256];


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

    init_sync();

    // Create some example sync data.
    for (int i=0; i<1024; i++) {
        sync_data[i] = ((uint32_t)i<<16) + (i<<6);
    }
}

// Create a static serial function so we can pass it to a command queue.
void serial_write(const char* s) {Serial.write(s);}
CommandQueue serial_commands(serial_write);

// Note: multiple command queues ccan operate simultaneously.
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
        // Serial.write((char)c); //echo for debugging
    }

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
