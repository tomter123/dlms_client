import re
with open('main/main.c', 'r') as f:
    text = f.read()

text = text.replace('UART_NUM_2', 'UART_NUM_1')

with open('main/main.c', 'w') as f:
    f.write(text)
