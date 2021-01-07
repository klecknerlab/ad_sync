#include "main.h"

#if !defined(COMMANDS_H)

#define COMMANDS_H 1

// Character types
enum CHAR_TYPES : int {WHITESPACE, EOL, ALPHA, DIGIT, BINSTART};

// States of the input character processor
enum CMD_CYCLES : int {IDLE, READ_WORD, READ_INT, READ_BIN, READ_BIN_LEN, CMD_ERROR};
enum BIN_WRITE_TARGETS : int {TARGET_NONE, TARGET_SYNC_DATA, TARGET_SERIAL1, TARGET_SERIAL2};

// Constants for the different commands
// The commands each has a 1 byte code, determined here.
// (By converting 1-4 words into a single uint32, we can quickly compare the commands)
enum CMD_NAMES : uint8_t {
    CMD_NONE, SYNC, READ, WRITE, ADDR, START, STOP, COUNT, RATE, ANA0, ANA1, SER1, SER2,
    TRIG, MASK, AVAIL, FLUSH, LED, CMD_ON, CMD_OFF, STAT, SET, SCALE, MODE, IDN, 
    CMD_INVALID //NOTE: If you add commands, CMD_INVALID MUST be LAST!
};

// Converts four characters into a uint32
#define CMD_UINT(chars) ( (((uint32_t)chars[0]) << 24) + (((uint32_t)chars[1]) << 16) + (((uint32_t)chars[2]) << 8) + ((uint32_t)chars[3]))
#define NUM_CMD (CMD_INVALID)
// These integers are the first 4 bytes of the command word converted into a
//   uint32.  This allows for quick compares!  Empty characters are zeros.
// NOTE: the order MUST be idential to the order in the enum above!
static const uint32_t CMD_WORDS[NUM_CMD] = {
    CMD_UINT("\0\0\0\0"),
    CMD_UINT("SYNC"),
    CMD_UINT("READ"),
    CMD_UINT("WRIT"),
    CMD_UINT("ADDR"),
    CMD_UINT("STAR"),
    CMD_UINT("STOP"),
    CMD_UINT("COUN"),
    CMD_UINT("RATE"),
    CMD_UINT("ANA0"),
    CMD_UINT("ANA1"),
    CMD_UINT("SER1"),
    CMD_UINT("SER2"),
    CMD_UINT("TRIG"),
    CMD_UINT("MASK"),
    CMD_UINT("AVAI"),
    CMD_UINT("FLUS"),
    CMD_UINT("\0LED"),
    CMD_UINT("\0\0ON"),
    CMD_UINT("\0OFF"),
    CMD_UINT("STAT"),
    CMD_UINT("\0SET"),
    CMD_UINT("SCAL"),
    CMD_UINT("MODE"),
    CMD_UINT("*IDN"),
};

// Routines for packing command words into a command "sentence"
// Single word commands don't need a macro; just compare the command constant
#define CMD2(c1, c2) ((c1<<8) + c2)
#define CMD3(c1, c2, c3) ((c1<<16) + (c2<<8) + c3)
#define CMD4(c1, c2, c3, c4) ((c1<<24) + (c2<<16) + (c3<<8) + c4)

// Maximum number of integers attached to a command.  Currently none use more than 3.
#define MAX_CMD_INTS 4

// Error types
enum CMD_ERRORS : int {
    NO_ERROR = 0,
    ERR_UNKNOWN_COMMAND,
    ERR_INVALID_COMMAND,
    ERR_EXTRA_BIN_DATA,
    ERR_INVALID_ARG,
    ERR_MALFORMED_ARG,
    ERR_INVALID_ADDR,
    ERR_INVALID_BIN_DATA_LEN,
    ERR_TOO_MANY_ARGS,
    ERR_INVALID_FREQ,
    ERR_MISSING_ARG,
    ERR_WRONG_NUM_ARGS1,
    ERR_WRONG_NUM_ARGS2,
};

// Error outputs for each type.
static const char* ERROR_STR[] = {
    "mystery error (this should never happen)",
    "unknown command",
    "invalid command",
    "included binary data, but command does not support it",
    "invalid argument value",
    "malformed argument (only integer arguments accepted)",
    "invalid address",
    "invalid binary data length",
    "too many arguments",
    "invalid freq (should be >=30 and <=700000)",
    "missing argument",
    "wrong number of arguments (should be 1)",
    "wrong number of arguments (should be 2)",
};

#define STR_BUF_LEN 65

// Command queue class
class CommandQueue {
    private:
        int cycle; 
        uint32_t word;
        int word_i;
        uint32_t command;
        uint32_t data[MAX_CMD_INTS];
        int num_data;
        int bin_data_len;
        int bin_target;
        int bin_data_written;
        uint8_t *sync_ptr;
        uint8_t *sync_end;
        void (*output_str)(const char*);
        char str_buffer[STR_BUF_LEN];

        void output_int(int x) {output_str(itoa(x, str_buffer, 10));}
        void output_eol() {output_str("\n");}
        void output_ok() {output_str("ok.\n");}
        void output_error();
        void output_float(float x);
        void finish_word();
        void execute_command();

    public:
        int error;

        CommandQueue(void (*func_ptr)(const char *)); // Define an output stream
        void reset(); // Resets the internal state; used to start a new command
        void process_char(char c); // Process a single input character
        const char* error_str(int error) {return ERROR_STR[error];} // Return an error string
        const char* error_str() {return ERROR_STR[error];} // Return the error string from the current error
};



#endif

