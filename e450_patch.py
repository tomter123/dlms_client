import re
with open('main/main.c', 'r') as f:
    text = f.read()

replacement = """
                      if (dlms_hdlc_parse_frame(rx_buf + start, end - start, &frame)) {
                          ESP_LOGI(TAG_PUSH, "Parsed Push Frame! Control: %02X, InfoLen: %u", frame.control, frame.info_len);
                          if (frame.info_len > 4) {
                              // Skip the proprietary 4-byte header (e.g. 02 00 00 6C or 03 00 00 5F)
                              uint8_t *ptr = frame.info + 4;
                              size_t remain = frame.info_len - 4;
                              
                              while (remain > 0) {
                                  // Is it a definition? (0x02 0x04 0x12 ...)
                                  if (remain >= 18 && ptr[0] == 0x02 && ptr[1] == 0x04 && ptr[2] == 0x12) {
                                      if (push_value_idx > 0) {
                                          // New push cycle started
                                          push_obis_count = 0;
                                          push_value_idx = 0;
                                      }
                                      
                                      // Extract OBIS
                                      if (ptr[5] == 0x09 && ptr[6] == 0x06) {
                                          uint8_t obis[6];
                                          memcpy(obis, ptr + 7, 6);
                                          char obis_str[32];
                                          sprintf(obis_str, "%u.%u.%u.%u.%u.%u", obis[0], obis[1], obis[2], obis[3], obis[4], obis[5]);
                                          ESP_LOGI(TAG_PUSH, "Parsed Definition: %s", obis_str);
                                          
                                          // Add to list
                                          if (push_obis_count < 30) {
                                              memcpy(push_obis_list[push_obis_count], obis, 6);
                                              push_obis_count++;
                                          }
                                      }
                                      ptr += 18;
                                      remain -= 18;
                                  } else {
                                      // It's a value!
                                      dlms_reading_t rdg;
                                      memset(&rdg, 0, sizeof(rdg));
                                      if (push_value_idx < push_obis_count) {
                                          memcpy(rdg.obis, push_obis_list[push_value_idx], 6);
                                      }
                                      
                                      int consumed = axdr_parse_value(ptr, remain, &rdg);
                                      if (consumed <= 0) {
                                          ESP_LOGE(TAG_PUSH, "Failed to parse A-XDR value, breaking");
                                          break;
                                      }
                                      
                                      if (push_value_idx < push_obis_count) {
                                          char obis_str[32];
                                          sprintf(obis_str, "%u.%u.%u.%u.%u.%u", rdg.obis[0], rdg.obis[1], rdg.obis[2], rdg.obis[3], rdg.obis[4], rdg.obis[5]);
                                          
                                          if (rdg.data_type == 0x09 || rdg.data_type == 0x0A) {
                                              ESP_LOGI(TAG_PUSH, "Value %s: %s", obis_str, rdg.str_val ? rdg.str_val : "null");
                                          } else {
                                              ESP_LOGI(TAG_PUSH, "Value %s: %f", obis_str, rdg.float_val != 0 ? rdg.float_val : (float)rdg.int_val);
                                          }
                                          // Publish to MQTT
                                          mqtt_publish_reading(&rdg);
                                      }
                                      
                                      push_value_idx++;
                                      ptr += consumed;
                                      remain -= consumed;
                                  }
                              }
                          }
                      } else {
                          ESP_LOGW(TAG_PUSH, "Failed to parse HDLC frame");
                      }
"""

text = re.sub(r'if \(dlms_hdlc_parse_frame.*?else \{\s*ESP_LOGW\(TAG_PUSH, "Failed to parse HDLC frame"\);\s*\}', replacement, text, flags=re.DOTALL)

with open('main/main.c', 'w') as f:
    f.write(text)
