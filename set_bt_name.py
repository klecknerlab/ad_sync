"""
A script to set the Bluetooth name of synchronizer device.

Run with "python set_bt_name.py [USB PORT] [BT NAME]"

Alternatively, if you do not specify BT NAME the bluetooth will be disabled.
"""

import ad_sync
import sys

if len(sys.argv) < 2:
    raise ValueError(
        """Run with "python set_bt_name.py [USB PORT] [BT NAME]"""
    )

port = sys.argv[1]

name = " ".join(sys.argv[2:])

# Open a synchronizer device.
sync = ad_sync.ADSync(port)

# Identify device
print("Device Identification: ", sync.idn().decode("UTF-8"))

# Set bluetooth name
print(sync.bluetooth(name).decode("UTF-8"))
sync.close()
