import re
with open('components/dlms_client/dlms_hdlc.c', 'r') as f:
    text = f.read()

replacement = """              // LLC for response (E6 E7 00) or UI push (E6 E6 00)
              if (info_field_len >= 3 && ((data[idx] == DLMS_LLC_RESPONSE_DSAP && (data[idx+1] == DLMS_LLC_RESPONSE_SSAP || data[idx+1] == DLMS_LLC_REQUEST_SSAP)) || data[idx] == 0xE0) && data[idx+2] == DLMS_LLC_CONTROL) {
                  idx += 3;
                  info_field_len -= 3;
              }
              // Double LLC quirk for E450 push
              if (info_field_len >= 3 && data[idx] == 0xE0 && data[idx+2] == 0x00) {
                  idx += 3;
                  info_field_len -= 3;
              }
"""

text = re.sub(r'// LLC for response.*?info_field_len -= 3;\s*\}', replacement, text, flags=re.DOTALL)

with open('components/dlms_client/dlms_hdlc.c', 'w') as f:
    f.write(text)
