import serial
import numpy as np
import time


class ADSyncError(Exception):
    pass


class ADSync:
    ANALOG_RANGE = 20
    ANALOG_MAX = 65536
    FREQ_MAX = 700000
    MAX_ADDR = 16384

    def __init__(self, port, baud=921600, timeout=0.5, debug=False):
        """
        Initialize a AD sync device.

        Parameters
        ----------
        port : string
            The serial port address of the device.

        Keywords
        --------
        baud : int (default: 921600)
        timeout : float (default: 0.5)
        debug : bool (default: False)
            If true, prints out all serial communcation with the device.
        """
        self.debug = debug
        self.ser = serial.Serial(port=None, baudrate=baud, timeout=timeout)
        self.ser.port = port
        self.ser.rts = False
        self.ser.dtr = False
        self.ser.open()
        self.byte_rate = baud / 10

    def reset(self):
        """
        Reset the device.

        Note: this works by activing the RTS bit on the serial port, which
        will *not* work over bluetooth!
        """
        self.ser.rts = True
        time.sleep(0.5)
        self.ser.rts = False
        time.sleep(1.0)
        self.ser.flush()

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
            elif arg is None:
                pass
            else:
                raise ADSyncError(
                    "command got data type (%s) it can't handle" % type(arg)
                )

        cmd = b' '.join(cmd) + b'\n'
        self.ser.write(cmd)

        self._last_cmd = cmd

        if self.debug:
            print("Wrote to device: ", cmd)

        return cmd

    def _reply(self):
        reply = self.ser.readline().strip()
        if reply.startswith(b'ERROR:'):
            lc = self._last_cmd
            if len(lc) > 31:
                lc = lc[:28] + b'...'
            raise ADSyncError(reply[6:].decode('utf-8').strip()
                + "\n(serial command: %s)" % repr(lc))

        if self.debug:
            print("Received from device ", reply)

        return reply

    def _bin_reply(self, err=True):
        c = self.ser.read()
        if c == b'>':
            header = self.ser.read_until(b'>')
            try:
                nbytes = int(header[:-1])
            except ValueError:
                raise ADSyncError(
                    'expected binary reply, device returned invalid size' %
                    (c + header)
                )
            data = self.ser.read(nbytes)
            # Should be a newline at the end -> lets flush it
            self.ser.readline()
            return data

        else:
            # This is not a binary reply!  Just treat it normally (prob. error)
            reply = c + self.ser.readline().strip()
            if reply.startswith(b'ERROR:'):
                raise ADSyncError(reply[6:].decode('utf-8').strip())
            elif err:
                raise ADSyncError(
                    'expected binary reply, device returned "%s"' % reply
                )
            return reply

    def start(self):
        "Start the sync output."
        self._cmd(b"SYNC START")
        return self._reply()

    def _analog(self, V, ref=None, clip=None):
        if ref is None:
            ref = -0.5 * self.ANALOG_RANGE
        if clip is None:
            clip = self.ANALOG_MAX-1

        iV = int((V - ref) / self.ANALOG_RANGE * self.ANALOG_MAX + 0.5)
        if clip:
            iV = min(max(iV, 0), clip)

        return iV

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
            raise ADSyncError("channel must be 0 or 1")
        if amplitude < 0:
            raise ADSyncError("Amplitude must be >= 0")
        if offset + amplitude > 10:
            raise ADSyncError("Analog scale out of range (too high)")
        if offset - amplitude < -10:
            raise ADSyncError("Analog scale out of range (too low)")

        amp = self._analog(2*amplitude, ref=0, clip=self.ANALOG_MAX)
        off = self._analog(offset - amplitude)
        self._cmd(b"ANA%d SCALE" % channel, amp, off)
        return self._reply()

    def analog_set(self, channel, V):
        '''
        Set the default output of an analog channel when it is not running
        in sync mode.  (See `mode` for more details.)

        Parameters
        ----------
        channel : int
            Analog channel, should be 0 or 1
        V : float
            Output volts, should be in the range (-10, 10)

        '''
        if channel not in (0, 1):
            raise ADSyncError("channel must be 0 or 1")
        if V > 10:
            raise ADSyncError("Analog output out of range (too high)")
        if V < -10:
            raise ADSyncError("Analog output out of range (too low)")

        self._cmd(b"ANA%d SET" % channel, self._analog(V))
        return self._reply()

    def mode(self, analog_mode=1, digital_mode=0):
        """
        Set the sync output mode.

        Keywords
        --------
        analog_mode : int (default: 1)
            0: No sync analog output; each channel goes to the default
            1: Analog 0 streams from sync data, analog 1 fixed value
            2: Analog 1 streams from sync data, analog 0 fixed value
            3: Both channels stream, alternating updates. Even sync data
                addresses go to analog 0, odd address to analog 1. Note that
                this halves the update rate of each analog channel, relative to
                the digital signals.
        digital_mode : int (default: 0)
            0: All 16 outputs derived from sync data
            1: "Or" mode. Channels 0-7 are logical "or"ed with channels 8-15.
                8-15 have the normal output. (Can be used to superimpose
                triggered and non-triggered signals.)
            2: "Swap" mode. Swaps channels 0-7 with 8-15. (Can be used to
                switch the digital signals without re-uploading.)
            3: "Swap-Or" mode.  Apply the swap and then the or operation.
        """
        self._cmd("SYNC MODE", analog_mode, digital_mode)
        return self._reply()

    def stop(self):
        "Stop the sync output."
        self._cmd(b"SYNC STOP")
        return self._reply()

    def write(self, addr, data, wait=True):
        """
        Write data to the sync memory.

        Parameters
        ----------
        addr : int
            The address to write to (0-16383)
        data : numpy array
            The data to write; should be a uint32 array.

        Keywords
        --------
        wait : bool (default: true)
            If True, wait for the write to finish before returning.
        """
        self._cmd("SYNC WRITE", addr, data)
        if wait:
            time.sleep((len(data) * 4 / self.byte_rate))
        return self._reply()

    def write_ad(self, addr, dig, ana, scale=1, wait=True):
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
        wait : bool (default: true)
            If True, wait for the write to finish before returning.
        """
        if len(dig) != len(ana):
            raise ValueError("digital and analog data should have same length")

        data = np.asarray(dig, 'uint32') << 16
        data += (np.clip(ana * (0.5/scale) + 0.5, 0, 1)
                 * (self.ANALOG_MAX-1)).astype('uint32')

        return self.write(addr, data, wait=wait)

    def rate(self, rate):
        """
        Set the rate for the sync outputs

        Parameters
        ----------
        rate : float
            The output rate in Hz.  Will be rounded to the nearest mHz.
        """
        ipart = int(rate)
        fpart = int((rate - ipart) * 1000 + 0.5)
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

    def trigger(self, count=1):
        """
        Trigger channels indicated by trigger mask.

        Keywords
        --------
        counts : int (default: 1)
            The number of periods to trigger for.
        """
        if not isinstance(count, int):
            raise ValueError("Count must be an integer!")

        self._cmd("TRIGGER", count)
        return self._reply()

    def trigger_mask(self, mask):
        """
        Set the trigger mask for the sync outputs.

        Parameters
        ----------
        mask : int
            The bitmask for the trigger.  If a bit is high, then this channel
            is triggered.  (For example: if mask = 38 = 0b00100110 = (1<<1) +
            (1<<2) + (1<<5) then channels 1, 2, and 5 would be triggered.)
        """
        if not isinstance(mask, int):
            raise ValueError("Mask must be an integer!")

        self._cmd("TRIGGER MASK", mask)
        self._reply()

    def led(self, r, g, b):
        """
        Set the indicator LED output.

        Parameters
        ----------
        r, g, b : ints
            The brightness of each channel, 0-255.  Output is gamma corrected.
        """
        self._cmd("LED", r, g, b)
        self._reply()

    def _send_bin(self, data):
        if isinstance(data, np.ndarray):
            data = bytes(data)
        self.ser.write(b'>%d>' % len(data))
        self.ser.write(data)

    def ser_write(self, channel, data):
        """
        Write data to a tunneled serial channel

        Parameters
        ----------
        channel : int (1 or 2)
            The serial channel to write to.
        data : string, bytes, or numpy array
            The string to write.
        """
        if channel not in (1, 2):
            raise ADSyncError("channel must be 1 or 2")

        if isinstance(data, (str, bytes)):
            # Convert to an aray, so that _cmd sends it as binary
            data = np.fromstring(data, dtype='u1')
        elif not isinstance(data, np.ndarray):
            raise ValueError(
                "data should be a string, bytes object or numpy array"
            )

        self._cmd("SER%d WRITE" % channel, data)
        return self._reply()

    def ser_baud(self, channel, baud):
        """
        Set the baud rate of a tunneled serial port

        *Note:* currently the serial channels only work in 8N1 mode with no
        RTS, DTR, or other channels.  This should work with 99% of modern
        serial devices, but may fail in certain cases

        Parameters
        ----------
        channel : int (1 or 2)
            The serial channel to write to.
        baud : int
            The baud rate of the channel.  Non-standard values may not work,
            and will fail silently
        """
        if channel not in (1, 2):
            raise ADSyncError("channel must be 1 or 2")

        self._cmd("SER%d RATE" % channel, baud)
        return self._reply()

    def ser_read(self, channel, max_bytes=None):
        """
        Read data from a tunneled serial channel

        Parameters
        ----------
        channel : int (1 or 2)
            The serial channel to read from

        Keywords
        --------
        max_bytes : int
            If specified, the maximum number of bytes to return.  If not
            specified, will return everything in the buffer
        """
        if channel not in (1, 2):
            raise ADSyncError("channel must be 1 or 2")

        self._cmd("SER%d READ" % channel, max_bytes)
        return self._bin_reply()

    def ser_flush(self, channel):
        """
        Flush data from a tunneled serial channel

        Parameters
        ----------
        channel : int (1 or 2)
            The serial channel to read from
        """
        if channel not in (1, 2):
            raise ADSyncError("channel must be 1 or 2")

        self._cmd("SER%d FLUSH" % channel)
        return self._reply()

    def ser_available(self, channel):
        """
        Return the number of bytes available in a tunneled serial channel

        Parameters
        ----------
        channel : int (1 or 2)
            The serial channel to check
        """
        if channel not in (1, 2):
            raise ADSyncError("channel must be 1 or 2")
        self._cmd("SER%d AVAIL" % channel)
        reply = self._reply()
        try:
            return(int(reply))
        except ValueError:
            raise ADSyncError(
                'SER AVAIL returned "%s" (should have been an int)' % reply
            )

    def bluetooth(self, name=None):
        """
        Enable or disable bluetooth connection.

        Note: once set, the device will remember the bluetooth settings upon
        reboot!

        Keywords
        --------
        name : str or None (default: None)
            If a string is passed, enabled the bluetooth connection with the
            given name.  If None is passed, disable the bluetooth connection
        """
        if name:
            name = np.fromstring(name, dtype='u1')
        else:
            name = None
        self._cmd("BLUETOOTH", name)
        return self._reply()

    def close(self):
        "Close the serial port associated with the device."
        self.ser.close()

    def __del__(self):
        self.close()


class SmoothRamp:
    def __init__(self, t0=0, ts=1, tr=0.5, tj=None, rate=None):
        '''Create a smooth ramp function.

        Keywords
        --------
        t0 : float (default: 0)
            The time between the start of the linear ramp and the active region
        ts : float (default: 1)
            The time of the linear scan region, not including t0
        tr : float (default: 0.5)
            The time to return to the start of the scan
        tj : float (default: tr / 8)
            The jerk timescale, which sets the timescale over which the
            acceleration ramps up.  Should be at most tr / 4
        rate : float (default: 2.0 / ts)
            The scan ramp rate.  Default scans between -1 and 1 over ts

        After creating a ramp function, you can call it like a function, where
        the input is the times at which to compute the ramp.  Optionally, you
        can specify a keyword `d=[0-3]` in the function call, which outputs a
        derivative of the ramp function.
        '''
        if tj is None:
            self.tj = tr / 8
        else:
            self.tj = tj
        if rate is None:
            rate = 2.0 / ts

        self.t0 = t0 # Time from end of acceleration to beginning of active scan
        self.ts = ts # Active scan time
        self.tl = t0 + ts # Linear ramp time
        self.ta = 0.5 * (tr - 4 * self.tj) # Full acceleration time
        self.tr = tr # Return time

        self.x0 = -rate * (t0 + 0.5*ts)
        self.v0 = rate
        self.amax = 4 * (self.tl + self.tr) * self.v0 / (self.tr * (self.tr - 2 * self.tj))
        self.jerk = self.amax / self.tj

        # Motion profiles for each stage: (dt, x0, v0, a0, j)
        self.profile = []

        x = self.x0
        v = self.v0
        a = 0

        # The profiles are determined only by the time and the (constant) jerk
        for i, (dt, j) in enumerate([
                    (self.tl, 0),
                    (self.tj, -self.jerk),
                    (self.ta, 0),
                    (2*self.tj, self.jerk),
                    (self.ta, 0),
                    (self.tj, -self.jerk)
                ]):
            self.profile.append((dt, x, v, a, j))
            x += v*dt + (a/2)*dt**2 + (j/6)*dt**3
            v += a*dt + (j/2)*dt**2
            a += j*dt

        self.T = self.tl + self.tr

    def __call__(self, t, d=0):
        dt = (np.asarray(t) % self.T)
        sort = np.argsort(dt)
        unsort = np.argsort(sort)
        t = dt[sort]
        x = np.zeros_like(dt)
        i0 = 0

        for (dt, x0, v, a, j) in self.profile:
            try:
                i1 = np.where(t > dt)[0][0]
            except:
                i1 = len(t)
            tt = t[i0:i1]
            if d == 0:
                x[i0:i1] = x0 + v*tt + (a/2)*tt**2 + (j/6) * tt**3
            elif d == 1:
                x[i0:i1] = v + a*tt + (j/2)*tt**2
            elif d == 2:
                x[i0:i1] = a + j*tt
            elif d == 3:
                x[i0:i1] = j
            else:
                raise ValueError('derivative (d) should be 0--3')

            t -= dt
            i0 = i1

        return np.array(x[unsort])
