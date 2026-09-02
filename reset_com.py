import serial, time, sys
s=serial.Serial('COM5', 115200, timeout=1)
s.setDTR(False)
s.setRTS(False)
time.sleep(0.1)
s.setDTR(True)
s.setRTS(True)
time.sleep(0.1)
s.setDTR(False)
s.setRTS(False)
end=time.time()+45
print('Reading COM5 after reset...')
while time.time()<end:
    sys.stdout.write(s.readline().decode('utf-8', errors='replace'))
    sys.stdout.flush()
