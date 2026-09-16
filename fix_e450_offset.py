import re
with open('main/main.c', 'r') as f:
    text = f.read()

# Change static int push_value_idx = 0; to static int push_value_idx = 1;
text = text.replace('static int push_value_idx = 0;', 'static int push_value_idx = 1; // Offset by 1 because definition 1 (Push setup) has no value')

# Also reset to 1 when a new frame arrives
text = text.replace('push_value_idx = 0;\n                                  }', 'push_value_idx = 1;\n                                  }')

with open('main/main.c', 'w') as f:
    f.write(text)
