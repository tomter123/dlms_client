import re
with open('main/main.c', 'r') as f:
    text = f.read()

replacement = """                              // Skip the proprietary 4-byte header (e.g. 02 00 00 6C or 03 00 00 5F)
                              uint8_t *ptr = frame.info + 4;
                              size_t remain = frame.info_len - 4;
                              
                              // If it starts with a DataNotification (0x0F), skip the invoke-ID (4 bytes) and datetime (13 bytes including length)
                              if (remain > 18 && ptr[0] == 0x0F) {
                                  ptr += 18;
                                  remain -= 18;
                              }
"""

text = re.sub(r'// Skip the proprietary 4-byte header.*?size_t remain = frame\.info_len - 4;', replacement, text, flags=re.DOTALL)

with open('main/main.c', 'w') as f:
    f.write(text)
