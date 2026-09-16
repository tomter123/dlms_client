import re
with open('main/main.c', 'r') as f:
    text = f.read()

replacement = """                              // If it starts with a DataNotification (0x0F), skip the invoke-ID (4 bytes) and datetime (13 bytes including length)
                              if (remain > 18 && ptr[0] == 0x0F) {
                                  ptr += 18;
                                  remain -= 18;
                              }
                              
                              // If it starts with Struct and Array of definitions (e.g. 02 0D 01 0D), skip those 4 bytes
                              if (remain > 4 && ptr[0] == 0x02 && ptr[2] == 0x01) {
                                  ptr += 4;
                                  remain -= 4;
                              }
"""

text = re.sub(r'// If it starts with a DataNotification.*?remain -= 18;\s*\}', replacement, text, flags=re.DOTALL)

with open('main/main.c', 'w') as f:
    f.write(text)
