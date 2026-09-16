import re
with open('c:/Users/MakerLab/Projekti/DLMS/main/main.c', 'r', encoding='utf-8') as f:
    content = f.read()
content = re.sub(r'ESP_LOGI\(TAG, "========================================\"\);\n\}\n    ESP_LOGI\(TAG, ".*?Main task is done.*?\n\}', 'ESP_LOGI(TAG, "========================================"\);\n}', content, flags=re.DOTALL)
with open('c:/Users/MakerLab/Projekti/DLMS/main/main.c', 'w', encoding='utf-8') as f:
    f.write(content)

