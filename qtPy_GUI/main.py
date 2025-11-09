import sys
import json
import serial
import serial.tools.list_ports
from PySide6.QtCore import QThread, Signal, Qt
from PySide6.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout,
    QHBoxLayout, QLabel, QComboBox, QPushButton,
    QTabWidget, QTableWidget, QTableWidgetItem,
    QMessageBox, QRadioButton, QButtonGroup
)

# ------------------ SERIAL READER THREAD ------------------
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


# ------------------ MAIN WINDOW ------------------
class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("ESP32 Task Monitor")
        self.resize(450, 550)

        self.tabs = QTabWidget()
        self.setCentralWidget(self.tabs)

        self.settings_tab = QWidget()
        self.monitor_tab = QWidget()
        self.tabs.addTab(self.settings_tab, "Settings")
        self.tabs.addTab(self.monitor_tab, "Monitor")

        self.serial_thread = None
        self.latest_tasks = []

        self.init_settings_tab()
        self.init_monitor_tab()

    # ---------------- SETTINGS TAB ----------------
    def init_settings_tab(self):
        main_layout = QVBoxLayout()
        form_layout = QVBoxLayout()

        # Baudrate row
        baud_layout = QHBoxLayout()
        baud_label = QLabel("Baudrate:")
        self.baudrate_combo = QComboBox()
        self.baudrate_combo.addItems(["9600", "115200", "230400"])
        self.baudrate_combo.setCurrentText("115200")  # Default selection
        baud_layout.addWidget(baud_label)
        baud_layout.addWidget(self.baudrate_combo)

        # COM port row
        port_layout = QHBoxLayout()
        port_label = QLabel("COM Port:")
        self.port_combo = QComboBox()
        self.refresh_ports()
        port_layout.addWidget(port_label)
        port_layout.addWidget(self.port_combo)

        # Connect button
        self.connect_btn = QPushButton("Connect")
        self.connect_btn.setFixedHeight(40)
        self.connect_btn.clicked.connect(self.start_serial)

        form_layout.addLayout(baud_layout)
        form_layout.addLayout(port_layout)
        form_layout.addWidget(self.connect_btn)
        form_layout.addStretch()
        main_layout.addLayout(form_layout)
        self.settings_tab.setLayout(main_layout)

    def refresh_ports(self):
        self.port_combo.clear()
        ports = [p.device for p in serial.tools.list_ports.comports()]
        self.port_combo.addItems(ports)

    # ---------------- MONITOR TAB ----------------
    def init_monitor_tab(self):
        layout = QVBoxLayout()

        # ---- Core usage labels (always visible) ----
        usage_layout = QHBoxLayout()
        self.core0_label = QLabel("Core 0: 0%")
        self.core1_label = QLabel("Core 1: 0%")

        # Style: color background for distinction
        self.core0_label.setStyleSheet("background-color: #004080; color: white; padding: 6px; border-radius: 5px;")
        self.core1_label.setStyleSheet("background-color: #008000; color: white; padding: 6px; border-radius: 5px;")

        usage_layout.addWidget(self.core0_label)
        usage_layout.addWidget(self.core1_label)
        layout.addLayout(usage_layout)

        # ---- Memory usage labels ----
        mem_layout = QHBoxLayout()
        self.heap_label = QLabel("Heap: 0 / 0 (0%)")
        self.internal_label = QLabel("Internal: 0 / 0 (0%)")
        self.heap_label.setStyleSheet("background-color: #800000; color: white; padding: 6px; border-radius: 5px;")
        self.internal_label.setStyleSheet("background-color: #A0522D; color: white; padding: 6px; border-radius: 5px;")

        mem_layout.addWidget(self.heap_label)
        mem_layout.addWidget(self.internal_label)
        layout.addLayout(mem_layout)

        # ---- Sorting radio buttons ----
        radio_layout = QHBoxLayout()
        sort_label = QLabel("Sort by:")
        self.name_radio = QRadioButton("Task Name")
        self.percent_radio = QRadioButton("Percentage")
        self.core_radio = QRadioButton("Core")  # <-- new radio button

        self.percent_radio.setChecked(True)

        self.sort_group = QButtonGroup()
        self.sort_group.addButton(self.name_radio)
        self.sort_group.addButton(self.percent_radio)
        self.sort_group.addButton(self.core_radio)

        self.name_radio.toggled.connect(self.apply_sorting)
        self.percent_radio.toggled.connect(self.apply_sorting)
        self.core_radio.toggled.connect(self.apply_sorting)

        radio_layout.addWidget(sort_label)
        radio_layout.addWidget(self.name_radio)
        radio_layout.addWidget(self.percent_radio)
        radio_layout.addWidget(self.core_radio)
        radio_layout.addStretch()
        layout.addLayout(radio_layout)

        # ---- Task table ----
        self.table = QTableWidget(0, 4)
        self.table.setHorizontalHeaderLabels(["Task Name", "Run Time", "Percentage", "Core"])

        layout.addWidget(self.table)
        self.monitor_tab.setLayout(layout)

    # ---------------- SERIAL HANDLING ----------------
    def start_serial(self):
        port = self.port_combo.currentText()
        baudrate = int(self.baudrate_combo.currentText())

        if not port:
            QMessageBox.warning(self, "Warning", "Please select a COM port first.")
            return

        self.serial_thread = SerialReaderThread(port, baudrate)
        self.serial_thread.data_received.connect(self.update_data)
        self.serial_thread.error_received.connect(self.show_error)
        self.serial_thread.serial_error.connect(self.show_error)
        self.serial_thread.start()

        # Switch automatically to Monitor tab
        self.tabs.setCurrentWidget(self.monitor_tab)

    def show_error(self, msg):
        QMessageBox.critical(self, "Error", msg)

    # ---------------- DATA HANDLING ----------------
    def update_data(self, data):
        # ---- Memory data ----
        if "heap_total" in data and "heap_free" in data:
            heap_total = data.get("heap_total", 0)
            heap_free = data.get("heap_free", 0)
            internal_total = data.get("internal_total", 0)
            internal_free = data.get("internal_free", 0)

            # Calculate used memory and percentages
            heap_used = heap_total - heap_free
            internal_used = internal_total - internal_free

            heap_percent_used = (heap_used / heap_total * 100) if heap_total else 0
            internal_percent_used = (internal_used / internal_total * 100) if internal_total else 0

            # Update labels (showing free/total and percent used)
            self.heap_label.setText(
                f"Heap: {heap_total - heap_free} / {heap_total}  ({heap_percent_used:.1f}%)"
            )
            self.internal_label.setText(
                f"Internal: {internal_total - internal_free} / {internal_total}  ({internal_percent_used:.1f}%)"
            )
            return

        # ---- Task data ----
        if "tasks" not in data:
            return

        self.latest_tasks = data["tasks"]
        self.apply_sorting()


    def apply_sorting(self):
        if not self.latest_tasks:
            return

        tasks = list(self.latest_tasks)

        # Sorting logic
        if self.name_radio.isChecked():
            tasks.sort(key=lambda t: t["task_name"].lower())
        elif self.core_radio.isChecked():
            tasks.sort(key=lambda t: t.get("core", -1))
        else:
            tasks.sort(key=lambda t: t["percentage"], reverse=True)

        # Compute total usage per core (only from tasks that have 'core')
        core_usage = {0: 0.0, 1: 0.0}
        for t in tasks:
            core = t.get("core")
            name = t.get("task_name", "").upper()
            if core in core_usage and not name.startswith("IDLE"):
                core_usage[core] += t.get("percentage", 0.0)

        # Update the core usage labels
        self.core0_label.setText(f"Core 0: {core_usage[0]:.1f}%")
        self.core1_label.setText(f"Core 1: {core_usage[1]:.1f}%")

        # Update table display
        self.table.setRowCount(len(tasks))
        for i, task in enumerate(tasks):
            self.table.setItem(i, 0, QTableWidgetItem(task["task_name"]))
            self.table.setItem(i, 1, QTableWidgetItem(str(task.get("run_time", 0))))
            self.table.setItem(i, 2, QTableWidgetItem(f"{task.get('percentage', 0)}%"))
            self.table.setItem(i, 3, QTableWidgetItem(str(task.get("core", "-"))))



# ------------------ RUN APP ------------------
if __name__ == "__main__":
    app = QApplication(sys.argv)
    win = MainWindow()
    win.show()
    sys.exit(app.exec())
