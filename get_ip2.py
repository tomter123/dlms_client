import serial
import time
import sys

ser = serial.Serial('COM9', 115200, timeout=0.1)

# toggle DTR/RTS to reset the board
ser.setDTR(False)
ser.setRTS(True)
time.sleep(0.1)
ser.setDTR(False)
ser.setRTS(False)

start_time = time.time()
while time.time() - start_time < 15:
    line = ser.readline().decode('utf-8', errors='ignore').strip()
    if line:
        if "got ip:" in line:
            print(line)
            sys.exit(0)
ser.close()
