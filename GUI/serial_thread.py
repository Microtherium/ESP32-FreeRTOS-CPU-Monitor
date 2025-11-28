
# serial_thread.py
import serial
import json
from qtpy.QtCore import QThread, Signal

class SerialReaderThread(QThread):
    data_received = Signal(dict)
    error_received = Signal(str)
    serial_error = Signal(str)

    def __init__(self, port, baudrate):
        super().__init__()
        self.port = port
        self.baudrate = baudrate
        self.running = True

    def run(self):
        try:
            with serial.Serial(self.port, self.baudrate, timeout=2) as ser:
                buffer = ""
                while self.running:
                    if ser.in_waiting > 0:
                        data = ser.read(ser.in_waiting).decode('utf-8', errors='ignore')
                        buffer += data
                        while '\n' in buffer:
                            line, buffer = buffer.split('\n', 1)
                            line = line.strip()
                            if not line:
                                continue
                            try:
                                parsed = json.loads(line)
                                if "error" in parsed:
                                    msg = parsed.get("error")
                                    code = parsed.get("code")
                                    if code:
                                        self.error_received.emit(f"{msg} (code: {code})")
                                    else:
                                        self.error_received.emit(msg)
                                else:
                                    self.data_received.emit(parsed)
                            except json.JSONDecodeError:
                                continue
        except serial.SerialException as e:
            self.serial_error.emit(str(e))

    def stop(self):
        self.running = False
