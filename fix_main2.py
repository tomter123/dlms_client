import re
with open('main/main.c', 'r') as f:
    text = f.read()

dlms_poll_old = """
            // Set up UART correctly (use the dynamic baud rate)
            uart_set_baudrate(UART_NUM_2, baud);
            
            dlms_client_config_t cfg = {
                .uart_num = UART_NUM_2,
                .client_addr = client_addr,
                .server_logical_addr = server_logical,
                .server_physical_addr = server_physical,
                .auth_level = strcmp(auth, "low") == 0 ? 1 : (strcmp(auth, "high") == 0 ? 2 : 0)
            };
"""

dlms_poll_new = """
            // Set up UART correctly (use the dynamic baud rate)
            uart_set_baudrate(UART_NUM_2, baud);
            
            dlms_client_config_t cfg = {
                .uart_port = UART_NUM_2,
                .client_address = client_addr,
                .server_logical = server_logical,
                .server_physical = server_physical,
                .auth_mode = strcmp(auth, "low") == 0 ? DLMS_AUTH_LOW : DLMS_AUTH_NONE
            };
"""
text = text.replace(dlms_poll_old.strip(), dlms_poll_new.strip())

with open('main/main.c', 'w') as f:
    f.write(text)
