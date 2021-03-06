#include "main.h"
#include "commands.h"

int char_type(char c) {
    if (c == 10) return EOL;
    if (c == 62) return BINSTART;
    if (c <= 32) return WHITESPACE;
    if ((c >= 48) && (c <= 57)) return DIGIT;
    else return ALPHA; // Treat everything else as "alphabetical", including symbols
}

CommandQueue::CommandQueue() {
    sync_end = (uint8_t*) (sync_data + SYNC_DATA_SIZE);
    reset();
}

int CommandQueue::output_float(float x) {
    int nbytes = 0;
    int ipart = (int)x;
    nbytes += output_int(ipart); //Note: the int gets put in str_buffer!

    // Find the number of digits by snooping in the str_buffer
    int dig;
    for (dig=0; dig<STR_BUF_LEN; dig++) {
        if (str_buffer[dig] == 0) {break;}
    }

    int dp = 7 - dig; // Print 7 sig figs
    if (dp > 0) {
        nbytes += output_buffer.write(".");
        float frac = x - ipart;
        for (int i=0; i<dp; i++) {frac *= 10;}
        // We *could* round the fractional part, but this *might* also affect the ipart, so it's too late!

        itoa((int)frac, str_buffer, 10);
        dig = 0;
        for (dig=0; dig<STR_BUF_LEN; dig++) {
            if (str_buffer[dig] == 0) {break;}
        }
        for (int i=dig; i<dp; i++) {output_buffer.write("0");}
        nbytes += output_buffer.write(str_buffer);
    }

    return nbytes;
}

void CommandQueue::reset() {
    cycle = IDLE;
    error = NO_ERROR;
    command = 0;
    for (int i=0; i<MAX_CMD_INTS; i++) {args[i] = 0;}
    num_args = 0;
    bin_data_len = 0;
    bin_target = TARGET_NONE;
    sync_ptr = (uint8_t*)sync_data;
    bin_data_written = 0;
}

void CommandQueue::finish_word() {
    // for (int i=0; i<4; i++) {led 0
    //     str_buffer[i] = (word >> ((3-i)*8)) & (0xFF);
    // }
    // str_buffer[4] = 0;
    // output_buffer.write(str_buffer);
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

int CommandQueue::output_error() {
    int nbytes = 0;
    nbytes += output_buffer.write("ERROR: ");
    nbytes += output_buffer.write(ERROR_STR[error]);
    nbytes += output_eol();
    return nbytes;
}

void CommandQueue::execute_command() {
    if (cycle == READ_WORD) {finish_word();}

    #ifdef CMD_DEBUG
        output_buffer.write("Command: ");
        output_int(command);
        output_buffer.write(", Args:");
        for (int i=0; i<num_args; i++) {
            output_buffer.write(" ");
            output_int(args[i]);
        }
        output_eol();
    #endif

    if (error) {
        output_error();
    } else {
        int i, n;

        #ifdef BLUETOOTH_ENABLED
            esp_err_t err;
        #endif

        switch (command) {
            case IDN:
                output_buffer.write("USB analog/digital synchronizer (version ");
                output_int(VERSION_MAJOR);
                output_buffer.write(".");
                output_int(VERSION_MINOR);
                output_buffer.write(").\n");
                break;

            case LED:
                startup_colors_active = 0;
                set_led_color((int)args[0], (int)args[1], (int)args[2]);
                output_ok();
                break;

            case CMD2(SER1, WRITE):
                output_buffer.write("Wrote ");
                output_int(bin_data_written);
                output_buffer.write(" bytes to serial 1.\n");
                break;

            case CMD2(SER2, WRITE):
                output_buffer.write("Wrote ");
                output_int(bin_data_written);
                output_buffer.write(" bytes to serial 2.\n");
                break;

            case CMD2(SER1, AVAIL):
                output_int(ser1_input.available);
                output_eol();
                break;

            case CMD2(SER2, AVAIL):
                output_int(ser2_input.available);
                output_eol();
                break;

            case CMD2(SER1, READ):
                n = min(ser1_input.available, (SER_BUFFER_SIZE - output_buffer.available) - 10);
                if (num_args >= 1) {n = min(args[0], n);}

                output_buffer.write(">");
                output_int(n);
                output_buffer.write(">");
                ser1_input.to_stream(output_buffer);
                output_eol();
                break;

            case CMD2(SER2, READ):
                n = min(ser2_input.available, (SER_BUFFER_SIZE - output_buffer.available) - 10);
                if (num_args >= 1) {n = min(args[0], n);}

                output_buffer.write(">");
                output_int(n);
                output_buffer.write(">");
                ser2_input.to_stream(output_buffer);
                output_eol();
                break;

            case CMD2(SER1, RATE):
                if (num_args != 1) {
                    error = ERR_WRONG_NUM_ARGS1;
                    output_error();
                } else {
                    Serial1.end();
                    Serial1.begin(args[0], SERIAL_8N1, RX1_PIN, TX1_PIN);
                    // Due to a hardware and/or software bug, resetting Serial 1 disables the i2s output on pins 16/17
                    // This can be fixed by resetting the pin config for I2S
                    i2s_set_pin(I2S_NUM_0, &pin_config);
                    Serial1.flush();
                    output_buffer.write("ok.\n");
                }
                break;

            case CMD2(SER2, RATE):
                if (num_args != 1) {
                    error = ERR_WRONG_NUM_ARGS1;
                    output_error();
                } else {
                    Serial2.end();
                    Serial2.begin(args[0], SERIAL_8N1, RX2_PIN, TX2_PIN);
                    Serial2.flush();
                    output_ok();
                }
                break;

            case CMD2(SER1, FLUSH):
                ser1_input.flush();
                ser1_output.flush();
                output_ok();
                break;

            case CMD2(SER2, FLUSH):
                ser2_input.flush();
                ser2_output.flush();
                output_ok();
                break;

            case CMD2(SYNC, STAT):
                output_buffer.write("I2S: wrote ");
                output_int(last_bytes_written);
                output_buffer.write(" bytes ");
                output_int(micros() - last_sync_update);
                output_buffer.write(" us ago (");
                output_int(buffer_update_time);
                output_buffer.write(" us to update buffer)\n");
                break;

            case CMD2(SYNC, WRITE):
                // Note: the data is actually written in the command character processing function!
                output_buffer.write("Wrote ");
                output_int(bin_data_written / 4);
                output_buffer.write(" samples to syncronous data, starting at address ");
                output_int(args[0]);
                i = bin_data_written % 4;
                if (i != 0) {
                    output_buffer.write(". (Warning: %d extra bytes written at end!)\n");
                } else {
                    output_buffer.write(".\n");
                }
                break;

            case CMD2(ANA0, SET):
                if (num_args != 1) {
                    error = ERR_WRONG_NUM_ARGS1;
                    output_error();
                } else {
                    ana0_set = args[0];
                    analog_update |= 1<<0;
                    output_ok();
                }
                break;

            case CMD2(ANA1, SET):
                if (num_args != 1) {
                    error = ERR_WRONG_NUM_ARGS1;
                    output_error();
                } else {
                    ana1_set = args[0];
                    analog_update |= 1<<1;
                    output_ok();
                }
                break;

            case CMD2(ANA0, SCALE):
                if (num_args != 2) {
                    error = ERR_WRONG_NUM_ARGS2;
                    output_error();
                } else {
                    ana0_multiplier = args[0];
                    ana0_offset = args[1];
                    output_ok();
                }
                break;

            case CMD2(ANA1, SCALE):
                if (num_args != 2) {
                    error = ERR_WRONG_NUM_ARGS2;
                    output_error();
                } else {
                    ana1_multiplier = args[0];
                    ana1_offset = args[1];
                    output_ok();
                }
                break;

            case CMD2(SYNC, MODE):
                if (num_args == 0) {
                    output_buffer.write("SYNC MODE ");
                    output_int(analog_sync_mode);
                    output_buffer.write(" ");
                    output_int(digital_sync_mode);
                    output_eol();
                } else if (args[0] < 4) {
                    analog_sync_mode = args[0];
                    analog_update |= (~analog_sync_mode) & 0b11;
                    digital_sync_mode = args[1];
                    output_ok();
                } else {
                    error = ERR_INVALID_ARG;
                    output_error();
                }
                break;

            case CMD2(SYNC, ADDR):
                if ((num_args == 2) && (args[0] < SYNC_DATA_SIZE) && (args[1] < SYNC_DATA_SIZE)) {
                    sync_start = args[0];
                    sync_cycles = args[1];
                    output_ok();
                } else {
                    error = ERR_INVALID_ADDR;
                    output_error();
                }
                break;

            case CMD2(SYNC, START):
                sync_active = 1;
                digitalWrite(OE_PIN, LOW);
                output_ok();
                break;

            case CMD2(SYNC, STOP):
                sync_active = 0;
                digitalWrite(OE_PIN, HIGH);
                output_ok();
                break;

            case CMD2(SYNC, RATE):
                if ((num_args == 1) || (num_args == 2)) {
                    float freq = (float)args[0];

                    if (num_args == 2) {
                        freq += 1E-3 * args[1];
                    }

                    if ((freq < MIN_FREQ) || (freq > MAX_FREQ)) {
                        error = ERR_INVALID_FREQ;
                        output_error();
                    } else {
                        freq = sync_freq(freq);
                        output_buffer.write("SYNC RATE = ");
                        output_float(freq);
                        output_buffer.write(" Hz\n");
                    }
                } else {
                    error = ERR_WRONG_NUM_ARGS2;
                    output_error();
                }
                break;

            case CMD2(TRIGGER, MASK):
                if (num_args == 1) {
                    trigger_mask = args[0];
                    output_ok();
                } else {
                    error = ERR_WRONG_NUM_ARGS1;
                    output_error();
                }
                break;

            case TRIGGER:
                if (num_args <= 1) {
                    if (num_args == 0) {trigger_count = 1;}
                    else {trigger_count = args[0];}
                    output_ok();
                } else {
                    error = ERR_WRONG_NUM_ARGS1;
                    output_error();
                }
                break;

            case BLUETOOTH:
                #ifdef BLUETOOTH_ENABLED
                    bt_name[bin_data_written] = 0; // Zero terminate string
                    err = bluetooth_set_name(bt_name);
                    if (err != ESP_OK) {
                        output_buffer.write("ERROR: failed to write bluetooth name to device (");
                        output_buffer.write(esp_err_to_name(err));
                        output_buffer.write(")\n");
                    } else {
                        if (bt_name[0]) {
                            output_buffer.write("Bluetooth enabled with name: ");
                            output_buffer.write(bt_name);
                            output_eol();
                        } else {
                            output_buffer.write("Bluetooth disabled.\n");
                        }
                    }
                #else
                    output_buffer.write("ERROR: Bluetooth currently disabled in firmare\n");
                #endif
                break;

            default:
                error = ERR_INVALID_COMMAND;
                output_error();
        }
    }

    reset();
}

void CommandQueue::process_char(char c) {
    // Handle binary read first, as this may be called many times
    if (cycle == READ_BIN) {
        switch (bin_target) {
            case TARGET_SYNC_DATA:
                *sync_ptr = (uint8_t)c;
                sync_ptr ++;
                if (sync_ptr >= sync_end) {
                    error = ERR_INVALID_ADDR;
                    bin_target = TARGET_NONE;
                }
                break;
            case TARGET_SERIAL1:
                ser1_output.write(c);
                break;
            case TARGET_SERIAL2:
                ser2_output.write(c);
                break;
            case TARGET_BT_NAME:
                #ifdef BLUETOOTH_ENABLED
                    if (bin_data_written >= BT_NAME_MAX_LENGTH) {
                        error = ERR_BT_NAME_TOO_LONG;
                        bin_target = TARGET_NONE;
                    } else {
                        bt_name[bin_data_written] = c;
                    }
                #endif
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
                        if ((num_args == 1) && (args[0] >= 0) && (args[0] < SYNC_DATA_SIZE)) {
                            bin_target = TARGET_SYNC_DATA;
                            sync_ptr = (uint8_t *)(sync_data + args[0]);
                        } else {
                            bin_target = TARGET_NONE;
                            error = ERR_INVALID_ADDR;
                        }
                        break;

                    case BLUETOOTH:
                        bin_target = TARGET_BT_NAME;
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
        return;
    } else if (ct == BINSTART) {
        bin_data_len = 0;
        cycle = READ_BIN_LEN;
        return;
    } else {//Regular character

        //If idle, determine how to process the character based on what it is.
        if (cycle == IDLE) {
            if (ct == DIGIT) {
                cycle = READ_INT;
                args[num_args] = 0;
                num_args ++;
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
            if (ct == WHITESPACE) {
                finish_word();
            } else {
                if ((c >= 97) && (c <= 122)) {c -= 32;} // Capitalize
                if (word_i < 4) {word = (word << 8) + (uint32_t)c;}
                word_i++;
            }
        } else if (cycle == READ_INT) {
            if (ct == WHITESPACE) {
                cycle = IDLE;
            } else if (ct == DIGIT) {
                if (num_args <= MAX_CMD_INTS) {
                    args[num_args-1] = args[num_args-1]*10 + ((int)c-48);
                } else {
                    cycle = CMD_ERROR;
                    error = ERR_TOO_MANY_ARGS;
                }
            } else {
                cycle = CMD_ERROR;
                error = ERR_MALFORMED_ARG;
            }
        }
    }
}
