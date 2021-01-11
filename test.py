"""
A test program for the AD Synchronizer.

This example uses essentially every function of the device -- you probably
don't need all of these in practice!

Run it with the com port specification as the first parameter.
"""

import ad_sync
import numpy as np
import time
import sys

if len(sys.argv) != 2:
    raise ValueError(
        """Run script with port specification as first (and only parameter)
Example: "python test.py COM3" """
    )

port = sys.argv[1]

NUM_SAMPLES = 256
OUTPUT_FREQ = 100

# Start address for data output
# In practice, you'd probably start at 0, but as an example...
START_ADDR = 1000

# Analog outputs are updated half a cycle before the digital outputs; we can
#   correct for the phase by accounting for this.
t = np.arange(NUM_SAMPLES) - 0.5
ana0 = np.sin(2*np.pi * t / NUM_SAMPLES)

# Digital outputs are specified bitwise.
dig = np.zeros(NUM_SAMPLES, dtype='uint32')
# Setting SD0 (sync'd digital output 0).  High pulse at t=0
dig[0] += 1 << 0
# Setting SD1.  High pulse at t=128
dig[NUM_SAMPLES//2] += 1 << 1
# Setting SD2.  We get a high pulse at t=0, 64, 128, 192
dig[np.arange(4) * NUM_SAMPLES // 4] += 1 << 2

# Open a synchronizer device.
sync = ad_sync.ADSync(port)

# Identify device
print(sync.idn())

# Stop any output, if active.
sync.stop()

# Write data
print(sync.write_ad(START_ADDR, dig, ana0))
# There is also the direct "write" command, but not recommended for general use

# Tell the device to use the data we just wrote
sync.addr(START_ADDR, NUM_SAMPLES)

# Set up the sync output
sync.mode(analog_mode=1)
# Analog mode 1 -> ana0 is driven by the synchronizer, ana1 is constant
# Default is digital mode 1 -> unmodified output of digital channels

# Set the output range for analog output 0 to go from -1 -- +3 volts
sync.analog_scale(0, 2, 1)

# Set the other analog output 1 to +3 V
sync.analog_set(1, 3)

# Set the output rate -- in this case we want to signal to run at 100 Hz, so
#   the output rate = NUM_SAMPLES * OUTPUT_FREQ = 25600 Hz
# The rate won't be exact; but it should be within a few PPM
print(sync.rate(NUM_SAMPLES * OUTPUT_FREQ))

# Trigger channel 2
sync.trigger_mask(2 << 1)

# Start the sync output
sync.start()

time.sleep(1)

# Trigger for 1 second = freq # of cycles
# While doing so, make the indicator red
sync.led(255, 0, 0)
sync.trigger(int(OUTPUT_FREQ))

# Wait for 1 second for the trigger to finish.
# This isn't super precise, but good enough
# ¯\_(ツ)_/¯
time.sleep(1)
sync.led(0, 0, 0)

# Print some stats
print(sync.stat())

# Lets try reading/writing from the attached serial ports
# Note: unlike other outputs, the tunneled serial ports are numbered 1 and 2
# There *is* a ser0, but this is the main USB communications, which you can't
#   access through a tunnel.
for n in (1, 2):
    # Set the baud rate of each channel
    sync.ser_baud(n, 115200)
    # And flush anything in there already
    sync.ser_flush(n)

# Write to each channel
for n in (1, 2):
    sync.ser_write(n, f"Hello from serial port {n}!\n")

# Give the serial port time to send/receive
time.sleep(0.1)

# Read from each channel.  To get something to appear, connect RX <-> TX on
#    two channels.
for n in (1, 2):
    reply = repr(sync.ser_read(n))
    print(f"Serial {n} returned: {reply}")
