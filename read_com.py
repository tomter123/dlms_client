import serial, time, sys
t=0
while t<10:
 try:
  s=serial.Serial('COM9', 115200, timeout=1); break
 except: time.sleep(1); t+=1
end=time.time()+90
print('Reading COM9...')
while time.time()<end:
    try: sys.stdout.write(s.readline().decode('utf-8', errors='replace')); sys.stdout.flush()
    except: pass
