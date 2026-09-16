import codecs

with open('components/web_server/web_server.c', 'r', encoding='utf-8') as f:
    c = f.read()

# Replace stack allocation in api_config_handler
c = c.replace('char mqtt_tpl[1024] = {0};', 'char *mqtt_tpl = calloc(1, 1024); if(!mqtt_tpl) { free(buf); return ESP_FAIL; }')
c = c.replace('char escaped[2048] = {0};', 'char *escaped = calloc(1, 2048); if(!escaped) { free(mqtt_tpl); free(buf); return ESP_FAIL; }')

# Add frees at the end of api_config_handler
c = c.replace('free(buf);\\n    return ESP_OK;', 'free(escaped);\\n    free(mqtt_tpl);\\n    free(buf);\\n    return ESP_OK;')

# Also replace decoded[1024] in post_handler just in case
c = c.replace('char decoded[1024];\\n            urldecode(decoded, val);', 'char *decoded = calloc(1, 2048);\\n            urldecode(decoded, val);')

c = c.replace('nvs_set_u8(nvs, "mqtt_en", 1);\\n            }\\n        }', 'nvs_set_u8(nvs, "mqtt_en", 1);\\n            }\\n            free(decoded);\\n        }')

with open('components/web_server/web_server.c', 'w', encoding='utf-8') as f:
    f.write(c)
