import numpy as np
from PyQt5 import QtCore
from PyQt5.QtWidgets import (QApplication, QCheckBox, QComboBox, QGridLayout,
        QMainWindow, QVBoxLayout, QLabel, QProgressBar, QPushButton, QWidget,
        QSpinBox, QDoubleSpinBox, qApp, QAction, QTabWidget, QFileDialog,
        QHBoxLayout, QStyle)
from PyQt5.QtGui import (QIcon)
import sys
import os
import serial.tools.list_ports
from .. import ADSync, SmoothRamp
import json
import re

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
            self.setCurrentIndex(default)

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


class ScanControls(ConfigTab):
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
        fpv = self.fpv.value()
        tr = 1E-3 * self.ramp_tr.value()

        ft0 = int(np.ceil(t0 * frame_rate))
        ftr = int(np.ceil(tr * frame_rate))
        total_frames = ft0 + fpv + ftr

        if self.sync is None:
            self.update_control_display()
            return

        oversample1 = int((ADSync.FREQ_MAX) // frame_rate)
        oversample2 = int(ADSync.MAX_ADDR // total_frames)
        oversample = min(oversample1, oversample2)
        # oversample = 2
        sample_rate = frame_rate * oversample

        samples = total_frames * oversample
        dig = np.zeros(samples, dtype='u2')

        camera_pulses = (ft0 + np.arange(fpv)) * oversample
        dig[camera_pulses] += 1 << 0 # Channel 0 is camera
        dig[camera_pulses] += 1 << 8 # Channel 8 is camera in align mode

        laser_pulses = np.arange(total_frames) * oversample
        dig[laser_pulses] += 1 << 1 # Channel 1 is the laser

        i0 = ft0 * oversample # sample # of first laser pulse in scan
        i1 = (ft0 + fpv - 1) * oversample # sample # of last laser pulse in scan
        im = (i0 + i1) // 2 # Mid point; may not align with frame, but thats ok
        align_pulses = np.array([i0, im, i1], dtype='i')

        dig[align_pulses] += 1 << 9 # Channel 1 in swap mode = laser

        t_a = (np.arange(samples) - 0.5) / sample_rate + self.galvo_delay.value() * 1E-3
        t_d = np.arange(samples) / sample_rate

        analog = SmoothRamp(t0=ft0, ts=fpv, tr=ftr)(t_a * frame_rate)
        self.analog_scale = abs(analog).max()
        analog /= self.analog_scale

        dig[ft0 * oversample] += 1 << 3 # Volume start signal
        dig[ft0 * oversample] += 1 << 11 # Volume start signal in alignment mode


        if self.flipped.isChecked():
            analog *= -1

        try:
            self.sync.stop()
            self.sync.led(255, 0, 255)
            self.sync.rate(sample_rate)
            response = self.sync.write_ad(0, dig, analog)

            m = re.match(b'Wrote (\d+) samples', response)

            self.sync.addr(0, samples)
            self.update_scale()
            self.update_active()

            if m and int(m.group(1)) == samples:
                self.upload_button.setEnabled(False)
                self.parent.statusBar().showMessage('Scan profile successfully uploaded!')
            else:
                self.parent.statusBar().showMessage('Syncronizer responded incorrectly to upload (disconnected?).')
                print(f"WARNING: unexpected response to sync data upload\n (received '{response.decode('utf-8')}')")
        except:
            print("Unexpected error:", sys.exc_info()[0])
            self.parent.statusBar().showMessage('ERROR: synchronizer failed to upload!')

        self.parent.main_controls.vps.setText(f'{frame_rate / total_frames:.1f} Hz')
        self.parent.main_controls.duty_cycle.setText(f'{100 * fpv / total_frames:.1f} %')
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

    def update_control_display(self):
        if not self.active:
            return

        frame_rate = 1E3 * self.frame_rate.value()
        t0 = 1E-3 * self.ramp_t0.value()
        fpv = self.fpv.value()
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

        self.tabs.addTab(self.main_controls, "Main")
        self.tabs.addTab(self.scan_controls, "Scan Setup")
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
