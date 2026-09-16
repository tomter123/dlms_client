import re
with open('main/main.c', 'r') as f:
    text = f.read()

# Fix the condition to detect new cycle: use push_obis_count instead, or just a flag!
# Actually, if we see a definition AND we previously saw a value.
replacement = """
                                    if (remain >= 18 && ptr[0] == 0x02 && ptr[1] == 0x04 && ptr[2] == 0x12) {
                                        if (push_value_idx > 1) { // We have parsed values, so this must be a new cycle
                                            // New push cycle started
                                            push_obis_count = 0;
                                            push_value_idx = 1;
                                        }
"""

text = re.sub(r'if \(remain >= 18 && ptr\[0\] == 0x02 && ptr\[1\] == 0x04 && ptr\[2\] == 0x12\) \{\s*if \(push_value_idx > 0\) \{\s*// New push cycle started\s*push_obis_count = 0;\s*push_value_idx = 1;\s*\}', replacement.strip(), text, flags=re.DOTALL)

with open('main/main.c', 'w') as f:
    f.write(text)
