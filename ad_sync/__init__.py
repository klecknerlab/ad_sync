import serial
import numpy as np

class ADSyncError(Exception):
    pass

class ADSync:
    def __init__(self, port, baud=115200, timeout=0.1):
        """
        Initialize a AD sync device.

        Parameters
        ----------
        port : string
            The serial port address of the device.

        Keywords
        --------
        baud : int (default: 115200)
        timeout : float (default: 0.1)
        """
        self.ser = serial.Serial(port=None, baudrate=baud, timeout=timeout)
        self.ser.port = port
        self.ser.rts = False
        self.ser.dtr = False
        self.ser.open()

    def idn(self):
        "Return identification string."
        self._cmd("*IDN")
        return(self._reply())

    def stat(self):
        "Return statistics on sync output."
        self._cmd("SYNC STAT")
        return(self._reply())

    def _cmd(self, *args):
        if self.ser.in_waiting:
            self.ser.reset_input_buffer()

        cmd = []
        for arg in args:
            if isinstance(arg, str):
                cmd.append(arg.strip().encode('utf-8'))
            elif isinstance(arg, bytes):
                cmd.append(arg.strip())
            elif isinstance(arg, int):
                cmd.append(b'%d' % arg)
            elif isinstance(arg, np.ndarray):
                arg = bytes(arg)
                cmd.append(b'>%d>' % len(arg) + arg)

        cmd = b' '.join(cmd) + b'\n'
        self.ser.write(cmd)

        return cmd

    def start(self):
        "Start the sync output."
        self._cmd(b"SYNC START")
        return self._reply()

    def analog_scale(self, channel, amplitude, offset):
        '''
        Set the output range of an analog output

        Parameters
        ----------
        channel : int
            The analog output channel (0 or 1)
        amplitude : float
            The amplitude of the output wave (1/2 the peak to peak volts)
        offset : float
            The offset of the output wave (volts)

        Note that the full range must lie between -10 and 10 V
        (offset + amplitude <= 10 V, offset - amplitude >= -10 V)
        '''

        if channel not in (0, 1):
            raise ADSyncError("Channel must be 0 or 1")
        if amplitude < 0:
            raise ADSyncError("Amplitude must be >= 0")
        if offset + amplitude > 10:
            raise ADSyncError("Analog scale out of range (too high)")
        if offset - amplitude < -10:
            raise ADSyncError("Analog scale out of range (too low)")

        amp = int(np.clip(amplitude / 10 * 65536 + 0.5, 0, 65536))
        off = int(np.clip(((offset-amplitude) / 20 + 0.5) * 65536, 0, 65535))
        print(self._cmd(b"ANA%d SCALE" % channel, amp, off))
        return self._reply()

    def stop(self):
        "Stop the sync output."
        self._cmd(b"SYNC STOP")
        return self._reply()

    def write(self, addr, data):
        """
        Write data to the sync memory.

        Parameters
        ----------
        addr : int
            The address to write to (0-16383)
        data : numpy array
            The data to write; should be a uint32 array.
        """
        self._cmd("SYNC WRITE", addr, data)
        return self._reply()

    def write_ad(self, addr, dig, ana, scale=1):
        """
        Combine analog and digital data into single data stream and write those
        to the sync memory.

        Parameters
        ----------
        addr : int
            The address to write to (0-16383)
        dig : numpy array (integer)
            The digital data to write
        ana : numpy array (float)
            The analog data to write.

        Keywords
        --------
        scale : float (default: 1)
            The scale of the analog data -- plus or minus this value gets
            mapped to the full range.  (I.e. for the default, -1 -> 0 and
            +1 -> 65535.)
        """
        if len(dig) != len(ana):
            raise ValueError("digital and analog data should have same length")

        data = np.asarray(dig, 'uint32') << 16
        data += (np.clip(ana * (0.5/scale) + 0.5, 0, 1) * 65535).astype('uint32')

        return self.write(addr, data)

    def rate(self, rate):
        """
        Set the rate for the sync outputs

        Parameters
        ----------
        rate : float
            The output rate in Hz.  Will be rounded to the nearest mHz.
        """
        ipart = int(rate)
        fpart = int((rate - ipart) * 100 + 0.5)
        self._cmd("SYNC RATE", ipart, fpart)
        return self._reply()

    def addr(self, start, count):
        """
        Set the address range for the sync outputs.

        Parameters
        ----------
        start : int
            The first address of the output.
        count : int
            The total number of ouptut data points
        """
        self._cmd("SYNC ADDR", start, count)
        return self._reply()

    def _reply(self):
        reply = self.ser.readline().strip()
        if reply.startswith(b'ERROR:'):
            raise ADSyncError(reply[6:].decode('utf-8').strip())
        return reply

    def _send_bin(self, data):
        if isinstance(data, np.ndarray):
            data = bytes(data)
        self.ser.write(b'>%d>' % len(data))
        self.ser.write(data)

    def close(self):
        "Close the serial port associated with the device."
        self.ser.close()

    def __del__(self):
        self.close()
