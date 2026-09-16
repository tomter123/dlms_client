import re
with open('main/main.c', 'r') as f:
    text = f.read()

text = text.replace('dlms_client_config_t dlms_cfg = {', '(void)wifi_events;\n    dlms_client_config_t dlms_cfg = {')
text = text.replace('.push_mode_enabled = false,\n    };', '.push_mode_enabled = false,\n    };\n    (void)dlms_cfg;')

with open('main/main.c', 'w') as f:
    f.write(text)
