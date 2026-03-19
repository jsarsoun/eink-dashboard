import serial, time, sys

port = 'COM7'
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
