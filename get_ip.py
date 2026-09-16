import serial
import time
import re

ser = serial.Serial('COM9', 115200, timeout=1)
start_time = time.time()
while time.time() - start_time < 10:
    line = ser.readline().decode('utf-8', errors='ignore').strip()
    if line:
        print(line)
        if "got ip:" in line:
            break
ser.close()
