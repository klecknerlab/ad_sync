import ad_sync
import numpy as np
import time

NUM_SAMPLES = 256
OUTPUT_FREQ = 1E3
START_ADDR = 0

# Analog outputs are updated half a cycle before the digital outputs; we can
#   correct for the phase by accounting for this.
t = np.arange(NUM_SAMPLES) - 0.5
ana0 = np.sin(2*np.pi * t / NUM_SAMPLES)

# Digital outputs are specified bitwise.
dig = np.zeros(NUM_SAMPLES, dtype='uint32')
# Setting SD0 (sync'd digital output 0).  High pulse at t=0
dig[0] += 1<<0
# Setting SD1.  High pulse at t=128
dig[NUM_SAMPLES//2] += 1<<1
# Setting SD2.  We get a high pulse at t=0, 64, 128, 192
dig[np.arange(4) * NUM_SAMPLES // 4] += 1<<2

# Open a synchronizer device.
# !!! You will probably need to change the COM Port !!!
sync = ad_sync.ADSync("COM3")

# Identify device
print(sync.idn())

# Stop any output, if active.
sync.stop()

# Write data
print(sync.write_ad(START_ADDR, dig, ana0))

# Tell the device to use the data we just wrote
sync.addr(START_ADDR, NUM_SAMPLES)

# Set the output rate -- in this case we want to signal to run at 100 Hz, so
#   the output rate = NUM_SAMPLES * OUTPUT_FREQ = 25600 Hz
print(sync.rate(NUM_SAMPLES * OUTPUT_FREQ))

# Set the output range to go from 1--3 volts
sync.analog_scale(0, 5, 5)

# Start the sync output
sync.start()

# Print some stats
print(sync.stat())
