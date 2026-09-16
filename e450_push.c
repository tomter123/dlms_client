static const char *TAG_PUSH = "e450_push";

void e450_push_task(void *pvParameters) {
    ESP_LOGI(TAG_PUSH, "Initializing E450 push listener on UART2 (RX=6, 2400 8E1)...");
    uart_config_t uart_config = {
        .baud_rate = 2400,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_EVEN,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_2, DLMS_RX_BUFFER_SIZE * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_2, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_2, UART_PIN_NO_CHANGE, 6, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    uint8_t *rx_buf = malloc(DLMS_RX_BUFFER_SIZE);
    size_t rx_len = 0;

    while (1) {
        int len = uart_read_bytes(UART_NUM_2, rx_buf + rx_len, DLMS_RX_BUFFER_SIZE - rx_len, pdMS_TO_TICKS(100));
        if (len > 0) {
            rx_len += len;
            ESP_LOGI(TAG_PUSH, "Received %d bytes. Buffer len: %d", len, rx_len);
            ESP_LOG_BUFFER_HEX(TAG_PUSH, rx_buf, rx_len);

            // Try to find HDLC frame
            size_t start=0, end=0;
            if (dlms_hdlc_find_frame(rx_buf, rx_len, &start, &end) == 0) {
                ESP_LOGI(TAG_PUSH, "Found HDLC frame from %u to %u", start, end);
                dlms_hdlc_frame_t frame;
                if (dlms_hdlc_parse_frame(rx_buf + start, end - start, &frame)) {
                    ESP_LOGI(TAG_PUSH, "Parsed Push Frame! Control: %02X, InfoLen: %u", frame.control, frame.info_len);
                    if (frame.info_len > 0) {
                        ESP_LOGI(TAG_PUSH, "Info payload:");
                        ESP_LOG_BUFFER_HEX(TAG_PUSH, frame.info, frame.info_len);
                    }
                } else {
                    ESP_LOGW(TAG_PUSH, "Failed to parse HDLC frame");
                }
                // Remove parsed frame from buffer
                memmove(rx_buf, rx_buf + end, rx_len - end);
                rx_len -= end;
            } else if (rx_len == DLMS_RX_BUFFER_SIZE) {
                ESP_LOGW(TAG_PUSH, "Buffer full without valid frame, clearing");
                rx_len = 0;
            }
        } else if (rx_len > 0) {
            // Timeout, maybe it's ASCII? Check if it starts with '/'
            if (rx_buf[0] == '/') {
                ESP_LOGI(TAG_PUSH, "ASCII Message detected:");
                rx_buf[rx_len] = 0;
                printf("%s
", rx_buf);
                rx_len = 0;
            }
        }
    }
}

