import numpy as np
from PyQt5 import QtCore
from PyQt5.QtWidgets import (QApplication, QCheckBox, QComboBox, QGridLayout,
        QMainWindow, QVBoxLayout, QLabel, QProgressBar, QPushButton, QWidget,
        QSpinBox, QDoubleSpinBox, qApp, QAction, QTabWidget, QFileDialog,
        QHBoxLayout, QStyle, QTextEdit, QLineEdit)
from PyQt5.QtGui import (QIcon)
import sys
import os
import serial.tools.list_ports
from .. import ADSync, SmoothRamp
import json
import re
import time

# Set the name in the menubar.
if sys.platform.startswith('darwin'):
    try:
        from Foundation import NSBundle
        bundle = NSBundle.mainBundle()
        if bundle:
            # app_name = os.path.splitext(os.path.basename(sys.argv[0]))[0]
            app_info = bundle.localizedInfoDictionary() or bundle.infoDictionary()
            if app_info:
                app_info['CFBundleName'] = 'MUVI Scan Synchronizer'
    except ImportError:
        pass


class ConfigTab(QWidget):
    def __init__(self, parent=None, *args, **kwargs):
        super().__init__(*args, **kwargs)

        self.parent = parent
        self.grid = QGridLayout()
        self.gridloc = 0
        self.setLayout(self.grid)

        self.settings = {}

        if hasattr(self, "_build"):
            self._build()


    def set_settings(self, settings, pop=True):
        for k, widget in self.settings.items():
            if k in settings:
                if pop:
                    val = settings.pop(k)
                else:
                    val = settings[k]

                if isinstance(widget, (QSpinBox, QDoubleSpinBox)):
                    widget.setValue(val)
                elif isinstance(widget, QComboBox):
                    i = widget.findText(val)
                    if i >= 0:
                        widget.setCurrentIndex(i)
                    else:
                        widget.setCurrentIndex(0)
                elif isinstance(widget, QCheckBox):
                    widget.setChecked(val)
                else:
                    print(f"Warning: can't set the value of unknown widget with name '{k}'")

        return settings


    def get_settings(self, settings=None):
        if settings is None:
            settings = {}

        for k, widget in self.settings.items():
            if isinstance(widget, (QSpinBox, QDoubleSpinBox)):
                settings[k] = widget.value()
            elif isinstance(widget, QComboBox):
                settings[k] = widget.itemText(widget.currentIndex())
            elif isinstance(widget, QCheckBox):
                settings[k] = widget.isChecked()
            else:
                print(f"Warning: could not get value of unknown widget with name '{k}'")

        return settings

    def add_control(self, label, value=1, min=0, max=100, step=1, decimals=2, update=None, tip=None, name=None):
        if decimals:
            control = QDoubleSpinBox()
        else:
            control = QSpinBox()

        control.setRange(min, max)
        control.setSingleStep(step)
        if decimals: control.setDecimals(decimals)
        control.setValue(value)

        if update is not None:
            control.valueChanged.connect(update)

        label = QLabel(label)
        label.setAlignment(QtCore.Qt.AlignRight | QtCore.Qt.AlignVCenter)

        self.grid.addWidget(label, self.gridloc, 0)
        self.grid.addWidget(control, self.gridloc, 1)
        self.gridloc += 1

        if tip is not None:
            tip = "<FONT COLOR=black>" + tip + "<\FONT>"
            control.setToolTip(tip)
            label.setToolTip(tip)

        if name is not None:
            self.settings[name] = control

        return control


    def add_checkbox(self, label, checked=False, update=None, tip=None, name=None):
        control = QCheckBox()
        control.setChecked(checked)

        if update is not None:
            control.stateChanged.connect(update)

        label = QLabel(label)
        label.setAlignment(QtCore.Qt.AlignRight | QtCore.Qt.AlignVCenter)

        self.grid.addWidget(label, self.gridloc, 0)
        self.grid.addWidget(control, self.gridloc, 1)
        self.gridloc += 1

        if tip is not None:
            tip = "<FONT COLOR=black>" + tip + "<\FONT>"
            control.setToolTip(tip)
            label.setToolTip(tip)

        if name is not None:
            self.settings[name] = control

        return control


    def add_display(self, label, default='-', tip=None):
        label = QLabel(label)
        label.setAlignment(QtCore.Qt.AlignRight | QtCore.Qt.AlignVCenter)
        self.grid.addWidget(label, self.gridloc, 0)

        display = QLabel('-')
        self.grid.addWidget(display, self.gridloc, 1)
        self.gridloc += 1

        if tip is not None:
            tip = "<FONT COLOR=black>" + tip + "<\FONT>"
            display.setToolTip(tip)
            label.setToolTip(tip)

        return display

    def add_combobox(self, label, items=[], default=None, update=None, tip=None, name=None, refresh=None):
        control = QComboBox()
        if items:
            control.addItems(items)

        if default is not None:
            control.setCurrentIndex(default)

        if update is not None:
            control.currentIndexChanged.connect(update)

        label = QLabel(label)
        label.setAlignment(QtCore.Qt.AlignRight | QtCore.Qt.AlignVCenter)

        self.grid.addWidget(label, self.gridloc, 0)

        if tip is not None:
            tip = "<FONT COLOR=black>" + tip + "<\FONT>"
            control.setToolTip(tip)
            label.setToolTip(tip)

        if name is not None:
            self.settings[name] = control

        if refresh:
            hbox = QHBoxLayout()
            refresh_button = QPushButton()
            refresh_button.setIcon(self.style().standardIcon(QStyle.SP_BrowserReload))
            refresh_button.clicked.connect(refresh)
            hbox.addWidget(control, 1)
            hbox.addWidget(refresh_button, 0)
            self.grid.addLayout(hbox, self.gridloc, 1)
        else:
            self.grid.addWidget(control, self.gridloc, 1)


        self.gridloc += 1

        return control

    def add_button(self, label, func=None, toggle=False, tip=None):
        button = QPushButton(label)

        if tip is not None:
            tip = "<FONT COLOR=black>" + tip + "<\FONT>"
            button.setToolTip(tip)

        if toggle:
            button.setCheckable(True)
            button.setChecked(pressed)
            button.setStyleSheet("QPushButton::checked{background-color:#66F;}")

        if func is not None:
            if toggle:
                button.toggled.connect(func)
            else:
                button.clicked.connect(func)

        self.grid.addWidget(button, self.gridloc, 0, 1, 2)
        self.gridloc += 1

        return button

    def add_two_buttons(self, label1, label2, func1=None, func2=None, tip1=None, tip2=None):
        button1 = QPushButton(label1)
        button2 = QPushButton(label2)

        if tip1 is not None:
            tip = "<FONT COLOR=black>" + tip1 + "<\FONT>"
            button1.setToolTip(tip)

        if tip2 is not None:
            tip = "<FONT COLOR=black>" + tip2 + "<\FONT>"
            button1.setToolTip(tip)

        if func1 is not None:
            button1.clicked.connect(func1)

        if func2 is not None:
            button2.clicked.connect(func2)

        self.grid.addWidget(button1, self.gridloc, 0)
        self.grid.addWidget(button2, self.gridloc, 1)
        self.gridloc += 1

        return button1, button2



class MainControls(ConfigTab):
    def _build(self):
        self.scan_range = self.add_control(
            'Scan Range (V):', 5, min=0, max=15, step=0.1, decimals=3,
            update=self.update_scale, name='scan_range',
            tip='The height (in volts) of the active section of the scan profile.  Note that the actual output will exceed this slightly, due to the smooth ramp return and the pre-scan ramp time!'
        )

        self.scan_offset = self.add_control(
            'Scan Offset (V):', 0, min=-10, max=10, step=0.1, decimals=3, update=self.update_scale, name='scan_offset',
            tip='The voltage at the center of the active section of the scan profile.'
        )

        self.align_mode = self.add_checkbox(
            'Alignment Mode:', False,
            update=self.update_align, name='alignment_mode',
            tip='If checked, output laser pulses in alignment mode (a pulse at the start, middle, and end of the active scan only)'
        )

        self.output_active = self.add_checkbox(
            'Outputs Active:', False,
            update=self.update_active, name='output_active',
            tip='If checked, outputs are active, otherwise they are not.'
        )

        self.led_active = self.add_checkbox(
            'LED indicator active:', True,
            update=self.update_active, name='led_active',
            tip='If checked, led indicator is active, otherwise it is always off.'
        )

        self.vps = self.add_display(
            'Volume Rate:',
            tip='The number of volumes captured per second.'
        )

        self.duty_cycle = self.add_display(
            'Scan Duty Cycle:',
            tip='The actual triggered framerate of the camera; will be slightly less than the requested frame rate to match the electronics time base.'
        )

        self.max_exposure = self.add_display(
            'Max Exposure Time:',
            tip='The maximum exposure time to set on the camera (1/framerate - 0.5 \u03bcs)'
        )

        self.trigger_button = self.add_button(
            'Fire Trigger', func=self.trigger,
            tip='Fire the trigger (digital port 3).'
        )
        self.trigger_button.setIcon(self.style().standardIcon(QStyle.SP_CommandLink))

        self.grid.addWidget(QWidget(), self.gridloc, 0, 1, 2)
        self.grid.setRowStretch(self.gridloc, 1)

    def update_scale(self):
        if hasattr(self.parent, "scan_controls"):
            self.parent.scan_controls.update_scale()

    def update_align(self):
        if hasattr(self.parent, "scan_controls"):
            self.parent.scan_controls.update_align()

    def update_active(self):
        if hasattr(self.parent, "scan_controls"):
            self.parent.scan_controls.update_active()

    def trigger(self):
        if hasattr(self.parent, "scan_controls"):
            self.parent.scan_controls.trigger()



class SerialTab(ConfigTab):
    BAUD_RATES = (9600, 14400, 19200, 38400, 57600, 115200, 128000, 256000)

    def __init__(self, port, *args, **kwargs):
        self.port = port
        super().__init__(*args, **kwargs)

    def _build(self):
        self.baudrate = self.add_combobox('BAUD:', list(map(str, self.BAUD_RATES)), default=0, update=self.set_baud)

        self.idn_button = self.add_button(
            'Identify Laser', func=self.laser_idn,
            tip='Send the "IDN" command to the laser.'
        )

        self.laser_on_button, self.laser_off_button = self.add_two_buttons(
            'Laser On', 'Laser Off', func1=self.laser_on, func2=self.laser_off,
            tip1='Turn on the laser connected to this port.',
            tip2='Turn off the laser connected to this port.'
        )

        self.ext_button, self.int_button = self.add_two_buttons(
            'Ext. Trigger', 'Int. Trigger', func1=self.external_trigger, func2=self.internal_trigger,
            tip1='Enable external triggering on the laser connected to this port.',
            tip2='Enable internal triggering on the laser connected to this port.'
        )

        self.serial_output = QTextEdit()
        self.serial_output.setReadOnly(True)
        self.grid.addWidget(self.serial_output, self.gridloc, 0, 1, 2)
        self.gridloc += 1

        self.read_button, self.clear_button = self.add_two_buttons(
            'Update', 'Clear', func1=self.read_serial, func2=self.clear_output,
            tip1='Read from the serial port.',
            tip2='Clear the output display.'
        )

        self.serial_input = QLineEdit()
        self.serial_input.returnPressed.connect(self.custom_command)
        self.grid.addWidget(self.serial_input, self.gridloc, 0, 1, 2)
        self.gridloc += 1

    def send_command(self, command):
        self.read_serial()
        self.serial_output.insertHtml(f'<font color="#00F">\u1405 {command}</font><br>')
        # if hasattr(self.parent, "scan_controls"):
        # time.sleep(0.1)
        self.parent.scan_controls.serial_write(self.port, command.encode('utf-8') + b"\r\n")

    def custom_command(self):
        self.send_command(self.serial_input.text())
        self.serial_input.clear()

    def laser_idn(self):
        self.send_command("*IDN?")

    def laser_on(self):
        self.send_command('MODE:RMT 1')
        self.send_command('ON')

    def laser_off(self):
        self.send_command('OFF')

    def external_trigger(self):
        self.send_command('QSW:PRF 0')

    def internal_trigger(self):
        self.send_command('QSW:PRF 10000')

    def set_baud(self):
        rate = int(self.baudrate.itemText(self.baudrate.currentIndex()))
        # if hasattr(self.parent, "scan_controls"):
        self.parent.scan_controls.serial_baud(self.port, rate)

    def read_serial(self, max_bytes=256):
        # if hasattr(self.parent, "scan_controls"):
        read = self.parent.scan_controls.serial_read(self.port, max_bytes)
        if read is not None:
            try:
                self.serial_output.insertHtml(read.decode('utf-8').replace('\n', '<br>'))
            except:
                self.serial_output.insertHtml(f'<nr><font color="#F00">!! corrupted input: {repr(read)} !!</font><br>')

    def clear_output(self):
        self.serial_output.clear()


class ScanControls(ConfigTab):
    _SER_DEBUG = True

    def _build(self):
        self.current_port = None
        self.sync = None
        self.active = False
        self.analog_scale = 1

        self.port_select = self.add_combobox(
            'Syncronizer Serial Port:',
            update=self.select_port, name='sync_port',
            refresh=self.update_ports,
            tip='Serial port to which the synchronizer board is connected.  You can update this list with File -> Update Serial Ports.'
        )
        self.update_ports()

        self.frame_rate = self.add_control(
            'Frame Rate (kHz):', 75, min=0.1, max=300, step=1, decimals=3,
            update=self.update_control_display, name='frame_rate_khz',
            tip='The camera frame rate; usually the maximum frame_rate for a given resolution on your camera.'
        )

        self.ramp_t0 = self.add_control(
            'Pre-scan Ramp Time (ms):', 0.2, min=0, max=100, step=0.1, decimals=2,
            update=self.update_control_display, name='ramp_t0_ms',
            tip='Delay from the start of the ramp to start capturing frames; used to ensure the galvo has settled into a linear ramp.'
        )

        self.galvo_delay = self.add_control(
            'Galvo Delay Compensation (ms):', 0.2, min=0, max=1, step=0.1, decimals=2,
            update=self.update_control_display, name='galvo_delay',
            tip='Delay applied to the galvo signal, to compensate for response.  (Analog output is shifted by this much earlier in time.)'
        )

        self.fpv = self.add_control(
            'Frames per Volume:', 512, min=0, max=2048, step=128, decimals=None,
            update=self.update_control_display, name='frames_per_volume',
            tip='The number of active frames in the scan.'
        )

        self.ramp_tr = self.add_control(
            'Ramp Return Time (ms):', 1.5, min=0, max=100, step=0.1, decimals=2,
            update=self.update_control_display, name='ramp_tr_ms',
            tip='Time for the ramp to return to the start.'
        )

        self.two_color = self.add_checkbox(
            'Two Color:', False,
            update=self.update_control_display, name='two_color',
            tip='If checked, output signals for two color imaging.  If not checked, run in one channel mode.'
        )

        self.flipped = self.add_checkbox(
            'Flip Scan Direction:', False,
            update=self.update_control_display, name='scan_flipped',
            tip='If checked, flip the direction of the scan.'
        )

        self.cont_frames = self.add_checkbox(
            'Continuous Frame Capture:', False,
            update=self.update_control_display, name='continuous_frame_capture',
            tip='If checked, record frames through the entire scan cycle (otherwise only records frames during the active period.'
        )

        self.double_pulse_1 = self.add_checkbox(
            'Double Pulse Laser 1:', False,
            update=self.update_control_display, name='double_pulse_1',
            tip='If checked, laser 1 outputs 2 pulses in rapid succession (note: this option is ignored if the framerate is >175 kHz, as the board cannot output fast enough!)'
        )

        self.double_pulse_2 = self.add_checkbox(
            'Double Pulse Laser 2:', False,
            update=self.update_control_display, name='double_pulse_2',
            tip='If checked, laser 2 outputs 2 pulses in rapid succession (note: this option is ignored if the framerate is >175 kHz, as the board cannot output fast enough!)'
        )

        self.upload_button = self.add_button(
            'Upload Scan Profile', func=self.upload_profile,
            tip='Upload the scan to the device.'
        )

        self.upload_button.setIcon(self.style().standardIcon(QStyle.SP_DialogApplyButton))

        self.vps = self.add_display(
            'Volume Rate:',
            tip='The number of volumes captured per second.'
        )

        self.duty_cycle = self.add_display(
            'Scan Duty Cycle:',
            tip='The actual triggered framerate of the camera; will be slightly less than the requested frame rate to match the electronics time base.'
        )

        # Add a stretchy empty spacer at the bottom.
        self.grid.addWidget(QWidget(), self.gridloc, 0, 1, 2)
        self.grid.setRowStretch(self.gridloc, 1)

        self.active = True
        self.update_control_display() # In this case, disable, as there is no sync board connected!



    def update_ports(self):
        ports = ['-none-'] + [p.device for p in serial.tools.list_ports.comports()]

        try:
            cpi = ports.index('-none-' if self.current_port is None else self.current_port)
        except ValueError:
            cpi = 0

        self.port_select.clear()
        self.port_select.addItems(ports)

        self.port_select.setCurrentIndex(cpi)

        # i = self.port_select.findText(self.current_port)
        # print(i)
        # if i >= 0:
        #     self.port_select.setCurrentIndex(i)
        # else:
        #     self.port_select.setCurrentIndex(0)
        #
        # # Refreshing after a clear seems to leave an empty item; remove it!
        # if self.port_select.count() > len(ports):
        #     self.removeItem(self.port_select.count())



    def select_port(self):
        port = self.port_select.itemText(self.port_select.currentIndex())

        # This gets triggered when you refresh/clear the list, just ignore!
        if not len(port):
            return

        if port == '-none-':
            port = None

        if port == self.current_port:
            return

        self.current_port = None
        if self.sync is not None:
            self.sync.close()

        if port is None:
            self.sync = None
            self.parent.statusBar().showMessage('Synchronizer not connected.')
        else:
            try:
                self.sync = ADSync(port)
                idn = self.sync.idn().decode('utf-8')
                if 'synchronizer' in idn.lower():
                    self.current_port = port
                    self.parent.statusBar().showMessage(f"Connected to sync board at {port}.")
                    self.update_scale()
                    self.update_align()
                else:
                    self.parent.statusBar().showMessage(f"ERROR: no sync board at {port}!")
                    self.sync.close()
                    self.sync = None
                    self.port_select.setCurrentIndex(0)
            except:
                self.parent.statusBar().showMessage(f"ERROR: could not open {port}!")
                if hasattr(self.sync, 'close'):
                    self.sync.close()
                self.sync = None
                self.port_select.setCurrentIndex(0)

        self.update_control_display()


    def upload_profile(self):
        frame_rate = 1E3 * self.frame_rate.value()
        t0 = 1E-3 * self.ramp_t0.value()
        channels = 2 if self.two_color.isChecked() else 1
        fpv = self.fpv.value()
        tr = 1E-3 * self.ramp_tr.value()

        ft0 = int(np.ceil(t0 * frame_rate))
        ftr = int(np.ceil(tr * frame_rate))
        total_frames = ft0 + fpv * channels + ftr

        # Ensure that each laser fires the same number of times per profile
        if total_frames % channels:
            extra_frames = channels - (total_frames % channels) # Round up
            ftr += extra_frames
            total_frames += extra_frames

        if self.sync is None:
            self.update_control_display()
            return

        oversample1 = int((ADSync.FREQ_MAX) // frame_rate)
        oversample2 = int(ADSync.MAX_ADDR // total_frames)
        oversample = min(oversample1, oversample2)
        sample_rate = frame_rate * oversample

        samples = total_frames * oversample
        dig = np.zeros(samples, dtype='u2')

        if self.cont_frames.isChecked():
            camera_pulses = np.arange(total_frames) * oversample
        else:
            camera_pulses = (ft0 + np.arange(fpv*channels)) * oversample

        dig[camera_pulses] += 1 << 0 # Channel 0 is camera
        dig[camera_pulses] += 1 << 8 # Channel 8 is camera in align mode

        laser_pulses = np.arange(0, total_frames, channels) * oversample
        i0 = ft0 * oversample # sample # of first laser pulse in scan
        i1 = i0 + (fpv - 1) * channels * oversample # sample # of last laser pulse in scan
        im = (i0 + i1) // 2 # Mid point; may not align with frame, but thats ok
        align_pulses = np.array([i0, im, i1], dtype='i')

        if oversample > 3:
            double_pulses = (
                self.double_pulse_1.isChecked(),
                self.double_pulse_2.isChecked()
            )
            ignore_dp = False
        else:
            double_pulses = (False, False)
            ignore_dp = self.double_pulse_1.isChecked() or self.double_pulse_2.isChecked()

        for i in range(channels):
            dig[laser_pulses + i*oversample] += 1 << (i+1) # Channel i+1 is laser i+1
            if double_pulses[i]:
                dig[laser_pulses + i*oversample + 2] += 1 << (i+1) # Channel i+1 is laser i+1
            dig[align_pulses + i*oversample] += 1 << (i+9) # Channel (i+1) in swap mode (alignment)

        t_a = (np.arange(samples) - 0.5) / sample_rate + self.galvo_delay.value() * 1E-3
        t_d = np.arange(samples) / sample_rate

        analog = SmoothRamp(t0=ft0, ts=fpv*channels, tr=ftr)(t_a * frame_rate)
        self.analog_scale = abs(analog).max()
        analog /= self.analog_scale

        dig[ft0 * oversample] += 1 << 3 # Volume start signal (triggered)
        dig[ft0 * oversample] += 1 << 11 # Volume start signal in alignment mode
        dig[ft0 * oversample] += 1 << 4 # Volume start signal (not triggered)
        dig[ft0 * oversample] += 1 << 12 # Volume start signal in alignment mode


        if self.flipped.isChecked():
            analog *= -1

        try:
            self.sync.stop()
            self.sync.led(255, 0, 255)
            self.sync.rate(sample_rate)
            self.sync.trigger_mask(1<<3)
            response = self.sync.write_ad(0, dig, analog)

            m = re.match(b'Wrote (\d+) samples', response)

            self.sync.addr(0, samples)
            self.update_scale()
            self.update_active()

            if m and int(m.group(1)) == samples:
                self.upload_button.setEnabled(False)
                if ignore_dp:
                    self.parent.statusBar().showMessage('Scan profile uploaded, but double pulse ignored.')
                else:
                    self.parent.statusBar().showMessage('Scan profile successfully uploaded!')
            else:
                self.parent.statusBar().showMessage('Syncronizer responded incorrectly to upload (disconnected?).')
                print(f"WARNING: unexpected response to sync data upload\n (received '{response.decode('utf-8')}')")
        except:
            print("Unexpected error:", sys.exc_info()[0])
            self.parent.statusBar().showMessage('ERROR: synchronizer failed to upload!')

        self.parent.main_controls.vps.setText(f'{frame_rate / total_frames:.1f} Hz')
        self.parent.main_controls.duty_cycle.setText(f'{100 * fpv*channels / total_frames:.1f} %')
        self.parent.main_controls.max_exposure.setText(f'{1E6 / frame_rate  - 0.5:.1f} \u03bcs')


    def update_scale(self):
        if self.sync is not None:
            scale = self.parent.main_controls.scan_range.value() / 2 * self.analog_scale
            offset = self.parent.main_controls.scan_offset.value()
            self.sync.analog_scale(0, scale, offset)

    def update_align(self):
        if self.sync is not None:
            self.sync.mode(1, 2 if self.parent.main_controls.align_mode.isChecked() else 0)
        self.update_active() # Updates the LED colors

    def update_active(self):
        if self.sync is not None:
            if self.parent.main_controls.output_active.isChecked():
                if self.parent.main_controls.led_active.isChecked():
                    if self.parent.main_controls.align_mode.isChecked():
                        self.sync.led(0, 0, 255)
                    else:
                        self.sync.led(0, 255, 0)
                else:
                    self.sync.led(0, 0, 0)

                self.sync.start()
            else:
                self.sync.led(0, 0, 0)
                self.sync.stop()

            self.sync.mode(1, 2 if self.parent.main_controls.align_mode.isChecked() else 0)

    def trigger(self):
        if self.sync is not None:
            self.sync.trigger()

    def serial_write(self, port, command):
        if self.sync is not None:
            write = self.sync.ser_write(port, command)
            if self._SER_DEBUG:
                print(port, '<-', repr(command), repr(write))
        else:
            return None

    def serial_read(self, port, max_bytes):
        if self.sync is not None:
            response = self.sync.ser_read(port, max_bytes)
            if self._SER_DEBUG:
                print(port, '->', repr(response))
            return response
        else:
            return None

    def serial_baud(self, port, baudrate):
        if self.sync is not None:
            if self._SER_DEBUG:
                print(port, ':', baudrate)
            self.sync.ser_baud(port, baudrate)
            return True
        else:
            return False

    def update_control_display(self):
        if not self.active:
            return

        frame_rate = 1E3 * self.frame_rate.value()
        t0 = 1E-3 * self.ramp_t0.value()
        channels = 2 if self.two_color.isChecked() else 1
        fpv = self.fpv.value() * channels
        tr = 1E-3 * self.ramp_tr.value()

        ft0 = int(np.ceil(t0 * frame_rate))
        ftr = int(np.ceil(tr * frame_rate))
        total_frames = ft0 + fpv + ftr

        self.vps.setText(f'{frame_rate / total_frames:.1f} Hz')
        self.duty_cycle.setText(f'{100 * fpv / total_frames:.1f} %')

        if hasattr(self, 'upload_button'):
            if self.sync is not None:
                self.upload_button.setEnabled(True)
            else:
                self.upload_button.setEnabled(False)


class MainWindow(QMainWindow):

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

        self.setWindowTitle('MUVI Scan Synchronizer')
        # self.setWindowIcon(QIcon('test_icon.png'))

        self.statusBar().showMessage('Synchronizer not connected.')

        # save_xml = QAction('Save MUVI XML', self)
        # save_xml.triggered.connect(self.save_xml)

        save_settings = QAction('&Save Scan Settings', self)
        save_settings.setShortcut('Ctrl+S')
        save_settings.triggered.connect(self.save_settings)

        load_settings = QAction('&Load Scan Settings', self)
        load_settings.setShortcut('Ctrl+L')
        load_settings.triggered.connect(self.load_settings)

        menubar = self.menuBar()
        file_menu = menubar.addMenu('&File')
        file_menu.addAction(save_settings)
        file_menu.addAction(load_settings)

        self.tabs = QTabWidget()
        self.setCentralWidget(self.tabs)

        self.main_controls = MainControls(self)
        self.scan_controls = ScanControls(self)
        self.serial1 = SerialTab(1, self)
        self.serial2 = SerialTab(2, self)

        self.tabs.addTab(self.main_controls, "Main")
        self.tabs.addTab(self.scan_controls, "Scan Setup")
        self.tabs.addTab(self.serial1, "Serial 1")
        self.tabs.addTab(self.serial2, "Serial 2")
        self.tabs.setCurrentIndex(1)

    def save_settings(self):
        settings = {}
        self.main_controls.get_settings(settings)
        self.scan_controls.get_settings(settings)
        fn, ext = QFileDialog.getSaveFileName(self, 'Save Settings', os.path.expanduser('~'), "MUVI synchronizer settings (*.json)")

        if fn:
            with open(fn, 'w') as f:
                json.dump(settings, f, indent=2)

            self.statusBar().showMessage(f"Saved settings to: {fn}.")

    def load_settings(self):
        fn, ext = QFileDialog.getOpenFileName(self, 'Load Settings', os.path.expanduser('~'), "MUVI synchronizer settings (*.json)")

        if fn:
            try:
                with open(fn, 'r') as f:
                    settings = json.load(f)
            except:
                self.statusBar().showMessage(f"Failed to load settings ({fn})!")
            else:
                self.main_controls.set_settings(settings)
                self.scan_controls.set_settings(settings)
                # print(settings)
                k = list(settings.keys())
                if k:
                    print(f"Warning: setup file contained unknown parameter(s): {k}")
                self.statusBar().showMessage(f"Settings successfully loaded from {fn}.")


def spawn():
    app = QApplication([])
    app.setStyle('Fusion')
    win = MainWindow()
    win.show()
    app.exec_()


if __name__ == "__main__":
    spawn()
