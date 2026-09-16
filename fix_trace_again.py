import codecs
with codecs.open('main/main.c', 'r', 'utf-8') as f:
    c = f.read()

trace_code = """
#define TRACE_BUF_SIZE 4096
static char g_trace_buf[TRACE_BUF_SIZE];
static int g_trace_head = 0;
static int g_trace_tail = 0;
static portMUX_TYPE s_trace_mux = portMUX_INITIALIZER_UNLOCKED;
static vprintf_like_t s_orig_vprintf = NULL;

static void append_trace_str(const char *str) {
    portENTER_CRITICAL(&s_trace_mux);
    while (*str) {
        g_trace_buf[g_trace_head] = *str++;
        g_trace_head = (g_trace_head + 1) % TRACE_BUF_SIZE;
        if (g_trace_head == g_trace_tail) {
            g_trace_tail = (g_trace_tail + 1) % TRACE_BUF_SIZE;
        }
    }
    portEXIT_CRITICAL(&s_trace_mux);
}

static int trace_vprintf(const char *fmt, va_list args) {
    if (strstr(fmt, "e450") || strstr(fmt, "dlms") || strstr(fmt, "main") || strstr(fmt, "RX:") || strstr(fmt, "TX:")) {
        char buf[256];
        va_list args_copy;
        va_copy(args_copy, args);
        int len = vsnprintf(buf, sizeof(buf), fmt, args_copy);
        va_end(args_copy);
        if (len > 0) {
            append_trace_str(buf);
        }
    }
    if (s_orig_vprintf) return s_orig_vprintf(fmt, args);
    return vprintf(fmt, args);
}

void clear_trace_buf(void) {
    portENTER_CRITICAL(&s_trace_mux);
    g_trace_head = 0;
    g_trace_tail = 0;
    portEXIT_CRITICAL(&s_trace_mux);
}
"""
c = c.replace('#include "esp_timer.h"', '#include "esp_timer.h"\\n' + trace_code)

# Since the previous fix script still hooked vprintf in app_main but missing the functions, this should fix it.
# Now I just need to fix get_debug_json replacing logic because DEBUG_BUF_SIZE was 2048 and my regex for old get_debug_json failed!
import re

get_debug_new = """
void get_debug_json(char *buf, size_t max_len) {
    char *hex = malloc(DEBUG_BUF_SIZE * 3 + 1);
    char *trace = malloc(TRACE_BUF_SIZE + 1);
    if (!hex || !trace) {
        if(hex) free(hex);
        if(trace) free(trace);
        snprintf(buf, max_len, "{}");
        return;
    }
    
    int h_idx = 0;
    xSemaphoreTake(g_debug_mutex, portMAX_DELAY);
    size_t curr = g_debug_tail;
    while (curr != g_debug_head && h_idx < (DEBUG_BUF_SIZE * 3 - 4)) {
        h_idx += snprintf(&hex[h_idx], 4, "%02X ", g_debug_buf[curr]);
        curr = (curr + 1) % DEBUG_BUF_SIZE;
    }
    xSemaphoreGive(g_debug_mutex);
    hex[h_idx] = '\\0';
    
    portENTER_CRITICAL(&s_trace_mux);
    int t_idx = 0;
    curr = g_trace_tail;
    while (curr != g_trace_head) {
        if (g_trace_buf[curr] == '"' || g_trace_buf[curr] == '\\\\') {
            trace[t_idx++] = '\\\\';
            trace[t_idx++] = g_trace_buf[curr];
        } else if (g_trace_buf[curr] == '\\n') {
            trace[t_idx++] = '\\\\';
            trace[t_idx++] = 'n';
        } else if (g_trace_buf[curr] == '\\r') {
            trace[t_idx++] = '\\\\';
            trace[t_idx++] = 'r';
        } else if (g_trace_buf[curr] >= 32 || g_trace_buf[curr] == '\\t') {
            trace[t_idx++] = g_trace_buf[curr];
        }
        curr = (curr + 1) % TRACE_BUF_SIZE;
    }
    trace[t_idx] = '\\0';
    portEXIT_CRITICAL(&s_trace_mux);
    
    snprintf(buf, max_len, "{\\"raw\\":\\"%s\\",\\"trace\\":\\"%s\\"}", hex, trace);
    free(hex);
    free(trace);
}
"""

c = re.sub(r'void get_debug_json\(char \*buf, size_t max_len\) \{.*?\}\n', get_debug_new.strip() + '\\n', c, flags=re.DOTALL)

with codecs.open('main/main.c', 'w', 'utf-8') as f:
    f.write(c)

