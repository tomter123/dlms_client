import re
with open('main/main.c', 'r') as f:
    text = f.read()

replacement = """void e450_push_task(void *pvParameters) {
    static uint8_t push_obis_list[30][6];
    static int push_obis_count = 0;
    static int push_value_idx = 0;
    
    ESP_LOGI(TAG_PUSH, "Initializing E450 push listener on UART2 (RX=6, 2400 8E1)...");"""

text = re.sub(r'void e450_push_task\(void \*pvParameters\)\s*\{\s*ESP_LOGI\(TAG_PUSH, "Initializing E450 push listener on UART2 \(RX=6, 2400 8E1\)\.\.\."\);', replacement, text, flags=re.DOTALL)

with open('main/main.c', 'w') as f:
    f.write(text)
