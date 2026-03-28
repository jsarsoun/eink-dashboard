import serial, serial.tools.list_ports, time, sys

ESP_VID_PIDS = {
    (0x303A, None),  # Espressif (XIAO ESP32-S3)
    (0x10C4, 0xEA60),  # Silicon Labs CP210x
    (0x1A86, 0x7523),  # CH340
    (0x0403, 0x6001),  # FTDI
}

def find_esp_port():
    for p in serial.tools.list_ports.comports():
        for vid, pid in ESP_VID_PIDS:
            if p.vid == vid and (pid is None or p.pid == pid):
                return p.device
    return None

port = find_esp_port()
if not port:
    print("No ESP32 device found. Available ports:")
    for p in serial.tools.list_ports.comports():
        print(f"  {p.device} — {p.description} (VID={p.vid:#06x} PID={p.pid:#06x})")
    sys.exit(1)

print(f"Watching {port} for 2 minutes...")
sys.stdout.flush()

end_time = time.time() + 120

while time.time() < end_time:
    try:
        with serial.Serial(port, 115200, timeout=1) as s:
            print(f"Connected to {port}")
            sys.stdout.flush()
            read_until = time.time() + 60
            while time.time() < read_until:
                try:
                    line = s.readline()
                    if line:
                        print(line.decode('utf-8', errors='replace').rstrip())
                        sys.stdout.flush()
                        read_until = time.time() + 60  # reset timer on each line received
                except serial.SerialException:
                    print("[disconnected]")
                    sys.stdout.flush()
                    break
    except serial.SerialException:
        time.sleep(0.3)

print("Done.")
