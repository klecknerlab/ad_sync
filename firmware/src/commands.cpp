#include "main.h"
#include "commands.h"

int char_type(char c) {
    if (c == 10) return EOL;
    if (c == 62) return BINSTART;
    if (c <= 32) return WHITESPACE;
    if ((c >= 48) && (c <= 57)) return DIGIT;
    else return ALPHA; // Treat everything else as "alphabetical", including symbols
}

CommandQueue::CommandQueue(void (*func_ptr)(const char *)) {
    output_str = func_ptr;
    sync_end = (uint8_t*) (sync_data + SYNC_DATA_SIZE);
    init();
    reset();
}

void CommandQueue::output_float(float x) {
    int ipart = (int)x;
    output_int(ipart); //Note: the int gets put in str_buffer!

    // Find the number of digits by snooping in the str_buffer
    int dig;
    for (dig=0; dig<STR_BUF_LEN; dig++) {
        if (str_buffer[dig] == 0) {break;}
    }

    int dp = 7 - dig; // Print 7 sig figs
    if (dp > 0) {
        output_str(".");
        float frac = x - ipart;
        for (int i=0; i<dp; i++) {frac *= 10;}
        // We *could* round the fractional part, but this *might* also affect the ipart, so it's too late!

        itoa((int)frac, str_buffer, 10);
        dig = 0;    
        for (dig=0; dig<STR_BUF_LEN; dig++) {
            if (str_buffer[dig] == 0) {break;}
        }
        for (int i=dig; i<dp; i++) {output_str("0");}
        output_str(str_buffer);
    }
}

void CommandQueue::reset() {
    cycle = IDLE;
    error = NO_ERROR;
    command = 0;
    for (int i=0; i<MAX_CMD_INTS; i++) {data[i] = 0;}
    num_data = 0;
    bin_data_len = 0;
    bin_target = TARGET_NONE;
    sync_data = (uint8_t*)sync_data;
    bin_data_written = 0;
}

void CommandQueue::finish_word() {
    // for (int i=0; i<4; i++) {led 0 
    //     str_buffer[i] = (word >> ((3-i)*8)) & (0xFF);
    // }
    // str_buffer[4] = 0;
    // output_str(str_buffer);
    // output_eol();

    int word_id = CMD_INVALID;
    for (int i=1; i < NUM_CMD; i++) {
        if (CMD_WORDS[i] == word) {
            word_id = i; break;
        }
    }

    command = (command << 8) + word_id;
    word_i = 0;
    word = 0;

    cycle = IDLE;
}

void CommandQueue::execute_command() {
    if (cycle == READ_WORD) {finish_word();}

    #ifdef CMD_DEBUG
        output_str("Command: ");
        output_int(command);
        output_str(", Data:");
        for (int i=0; i<num_data; i++) {
            output_str(" ");
            output_int(data[i]);
        }
        output_eol();
    #endif

    if (error) {
        output_str("ERROR: ");
        output_str(ERROR_STR[error]);
        output_eol();
    } else {
        int i, n;
        char tmp_char;

        switch (command) {
            case IDN:
                output_str("USB analog/digital synchronizer (version ");
                output_int(VERSION_MAJOR);
                output_str(".");
                output_int(VERSION_MINOR);
                output_str(").\n");
                break;

            case LED:
                for (i=0; i<3; i++) {
                    ledcWrite(i, (LED_LUT[min(data[i], 255)] * LED_TRIM[i]) >> 16);
                }
                output_ok();
                break;

            case CMD2(SER1, WRITE):
                output_str("Wrote ");
                output_int(bin_data_written);
                output_str(" bytes to serial 1.\n");
                break;

            case CMD2(SER2, WRITE):
                output_str("Wrote ");
                output_int(bin_data_written);
                output_str(" bytes to serial 2.\n");
                break;

            case CMD2(SER1, AVAIL):
                output_int(ser1_buffer_count);
                output_eol();
                break;

            case CMD2(SER2, AVAIL):
                output_int(ser2_buffer_count);
                output_eol();
                break;

            case CMD2(SER1, READ):
                if (num_data < 1) {n = ser1_buffer_count;}
                else {n = min(data[0], ser1_buffer_count);}
                output_str(">");
                output_int(n);
                output_str(">");
                tmp_char = ser1_buffer[n];
                ser1_buffer[n] = 0;
                output_str(ser1_buffer);
                ser1_buffer[n] = tmp_char;
                output_eol();

                // Shift data back if needed
                for (int i=(ser1_buffer_count-n); i>=0; i--) {
                    ser1_buffer[i] = ser1_buffer[i+n];
                }
                ser1_buffer_count -= n;

                break;

            case CMD2(SER2, READ):
                if (num_data < 1) {n = ser2_buffer_count;}
                else {n = min(data[0], ser2_buffer_count);}
                output_str(">");
                output_int(n);
                output_str(">");
                tmp_char = ser2_buffer[n];
                ser2_buffer[n] = 0;
                output_str(ser2_buffer);
                ser2_buffer[n] = tmp_char;
                output_eol();

                // Shift data back if needed
                for (int i=(ser2_buffer_count-n); i>=0; i--) {
                    ser1_buffer[i] = ser2_buffer[i+n];
                }
                ser2_buffer_count -= n;

                break;

            case CMD2(SER1, RATE):
                if (num_data < 1) {
                    output_str("ERROR: rate not specified!\n");
                } else {
                    Serial1.end();
                    Serial1.begin(data[0], SERIAL_8N1, RX1_PIN, TX1_PIN);
                    Serial1.flush();
                    output_str("ok.\n");
                }
                break;

            case CMD2(SER2, RATE):
                if (num_data < 1) {
                    output_str("ERROR: rate not specified!\n");
                } else {
                    Serial2.end();
                    Serial2.begin(data[0], SERIAL_8N1, RX2_PIN, TX2_PIN);
                    Serial2.flush();
                    output_ok();
                }
                break;

            case CMD2(SER1, FLUSH):
                ser1_buffer_count = 0;
                output_ok();
                break;

            case CMD2(SER2, FLUSH):
                ser2_buffer_count = 0;
                output_ok();
                break;

            case CMD2(SYNC, STAT):
                output_str("I2S: wrote ");
                output_int(last_bytes_written);
                output_str(" bytes ");
                output_int(cycles_since_write);
                output_str(" cycles ago (");
                output_int(buffer_update_time);
                output_str(" us to update buffer)\n");
                break;

            case CMD2(SYNC, WRITE):
                output_str("Wrote ");
                output_int(bin_data_written / 4);
                output_str(" samples to syncronous data, starting at address ");
                output_int(data[0]);
                i = bin_data_written % 4;
                if (i != 0) {
                    output_str(". (Warning: %d extra bytes written at end!)\n");
                } else {
                    output_str(".\n");
                }
                break;

            case CMD2(ANA0, SET):
                if (num_data != 1) {
                    output_str("ERROR: analog set should have one data int!\n");
                } else {
                    ana0_set = data[0];
                    analog_update |= 1<<0;
                    output_ok();
                }
                break;

            case CMD2(ANA1, SET):
                if (num_data != 1) {
                    output_str("ERROR: analog set should have one data int!\n");
                } else {
                    ana1_set = data[0];
                    analog_update |= 1<<1;
                    output_ok();
                }
                break;

            case CMD2(ANA0, SCALE):
                if (num_data != 2) {
                    output_str("ERROR: analog scale command must have two data ints!\n");
                } else {
                    ana0_multiplier = data[0];
                    ana0_offset = data[1];
                    output_ok();
                }
                break;

            case CMD2(ANA1, SCALE):
                if (num_data != 2) {
                    output_str("ERROR: analog scale command must have two data ints!\n");
                } else {
                    ana1_multiplier = data[0];
                    ana1_offset = data[1];
                    output_ok();
                }
                break;

            case CMD2(SYNC, MODE):
                if (num_data == 0) {
                    output_str("SYNC MODE ");
                    output_int(analog_sync_mode);
                    output_str(" ");
                    output_int(digital_sync_mode);
                    output_eol();
                } else if (data[0] < 4) {
                    analog_sync_mode = data[0];
                    analog_update |= (~analog_sync_mode) & 0b11;
                    digital_sync_mode = ((num_data == 1) || data[1]) ? 1 : 0;
                    output_ok();
                } else {
                    output_str("ERROR: invalid sync mode!\n");
                }
                break;
            
            case CMD2(SYNC, ADDR):
                if ((num_data == 2) && (data[0] < SYNC_DATA_SIZE) && (data[1] < SYNC_DATA_SIZE)) {
                    sync_start = data[0];
                    sync_cycles = data[1];
                    output_ok();
                } else {
                    output_str("ERROR: invalid start/end address fo SYNC CYCLE (both should be < ");
                    output_int(SYNC_DATA_SIZE);
                    output_str(")\n");
                }
                break;

            case CMD2(SYNC, CMD_ON):
                sync_active = 1;
                digitalWrite(OE_PIN, LOW);
                output_ok();
                break;

            case CMD2(SYNC, CMD_OFF):
                sync_active = 0;
                digitalWrite(OE_PIN, HIGH);
                output_ok();
                break;

            case CMD2(SYNC, RATE):
                if (num_data == 1) {
                    float freq = sync_freq((float)data[0]);
                    output_str("SYNC RATE = ");
                    output_float(freq);
                    output_str(" Hz\n");
                } else {
                    output_str("ERROR: SYNC RATE should have 1 argument.\n");
                }
                break;

            default:
                output_str("ERROR: unknown command (ignored)\n");
        }
    }

    reset();
}

void CommandQueue::process_char(char c) {
    // Handle binary read first, as this may be called many times
    if (cycle == READ_BIN) {
        switch (bin_target) {
            case TARGET_SYNC_DATA:
                *sync_data = (uint8_t)c;
                sync_data ++;
                if (sync_data >= sync_end) {
                    error = ERR_INVALID_ADDR;
                    bin_target = TARGET_NONE;
                }
                break;
            case TARGET_SERIAL1:
                Serial1.write(c);
                break;
            case TARGET_SERIAL2:
                Serial2.write(c);
                break;
        }

        bin_data_written ++;
        if (bin_data_written >= bin_data_len) {
            cycle = IDLE;
        }

        return;
    }

    int ct = char_type(c);

    // Are we reading a binary length?  In this case, don't process
    //  normally, just accept digits and whitespace.
    if (cycle == READ_BIN_LEN) {
        switch (ct) {
            case DIGIT:
                bin_data_len = bin_data_len * 10 + (c-48);
                break;

            case BINSTART:
                cycle = READ_BIN;
                if (error) {
                    bin_target = TARGET_NONE;
                } else switch (command) {
                    case CMD2(SER1, WRITE):
                        bin_target = TARGET_SERIAL1;
                        break;
                    case CMD2(SER2, WRITE):
                        bin_target = TARGET_SERIAL2;
                        break;
                    case CMD2(SYNC, WRITE):
                        if ((data[0] >= 0) && (data[0] < SYNC_DATA_SIZE)) {
                            bin_target = TARGET_SYNC_DATA;
                            sync_data = (uint8_t*) (sync_data + data[0]);
                        } else {
                            bin_target = TARGET_NONE;
                            error = ERR_INVALID_ADDR;
                        }
                        break;
                    default:
                        cycle = CMD_ERROR;
                        error = ERR_EXTRA_BIN_DATA;
                }
                break;

            default:
                cycle = CMD_ERROR;
                error = ERR_INVALID_BIN_DATA_LEN;
        }

        return;
    }

    if (ct == EOL) {
        execute_command();
    } else if (ct == BINSTART) {
        bin_data_len = 0;
        cycle = READ_BIN_LEN;
    } else {//Regular character

        //If idle, determine how to process the character based on what it is.
        if (cycle == IDLE) {
            if (ct == DIGIT) {
                cycle = READ_INT;
                data[num_data] = 0;
                num_data ++;
                // Don't return -- this char is processed below.
            } else if (ct == ALPHA) {
                cycle = READ_WORD;
                word_i = 0;
                word = 0;
                // Don't return -- this char is processed below.
            } else if (ct == WHITESPACE) {
                return;
            } else { //This should never happen, but just in case...
                cycle = CMD_ERROR;
                error = ERR_INVALID_COMMAND;
                return;
            }
        }

        if (cycle == READ_WORD) {
            if (ct == WHITESPACE) {finish_word();}
            else {
                if ((c >= 97) && (c <= 122)) {c -= 32;} // Capitalize
                if (word_i < 4) {word = (word << 8) + (uint32_t)c;}
                word_i++;
            }
        } else if (cycle == READ_INT) {
            if (ct == WHITESPACE) {cycle = IDLE;}
            else if (ct == DIGIT) {
                    if (num_data <= MAX_CMD_INTS) {
                        data[num_data-1] = data[num_data-1]*10 + ((int)c-48);
                    } else {
                        cycle = CMD_ERROR;
                        error = ERR_TOO_MUCH_DATA;
                    }
                }
            else {
                cycle = CMD_ERROR;
                error = ERR_INVALID_DATA;
            }
        }
    }
}
