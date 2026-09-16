import re
with open('main/main.c', 'r') as f:
    text = f.read()

replacement = """                  // Remove parsed frame from buffer, keeping the trailing flag for back-to-back frames
                  memmove(rx_buf, rx_buf + end - 1, rx_len - (end - 1));
                  rx_len -= (end - 1);"""

text = re.sub(r'// Remove parsed frame from buffer\s*memmove\(rx_buf, rx_buf \+ end, rx_len - end\);\s*rx_len -= end;', replacement, text, flags=re.DOTALL)

with open('main/main.c', 'w') as f:
    f.write(text)
