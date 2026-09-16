import codecs

with codecs.open('components/web_server/html/index.html', 'r', 'utf-8') as f:
    html = f.read()

# 1. Update Debugging HTML structure
debug_html_old = """
            <div style="background:#1e1e1e; color:#00ff00; padding:10px; border-radius:4px; font-family:monospace; min-height:200px; max-height:400px; overflow-y:auto; word-break:break-all;" id="debug_out">
            </div>
"""
debug_html_new = """
            <div style="display:flex; gap:20px;">
                <div style="flex:1;">
                    <h4>Parsed Logs & Trace (ASCII)</h4>
                    <div style="background:#1e1e1e; color:#d4d4d4; padding:10px; border-radius:4px; font-family:monospace; font-size:12px; height:400px; overflow-y:auto; white-space:pre-wrap;" id="trace_out"></div>
                </div>
                <div style="flex:1;">
                    <h4>Raw Serial Stream (HEX)</h4>
                    <div style="background:#1e1e1e; color:#00ff00; padding:10px; border-radius:4px; font-family:monospace; font-size:12px; height:400px; overflow-y:auto; word-break:break-all;" id="debug_out"></div>
                </div>
            </div>
"""
html = html.replace(debug_html_old.strip(), debug_html_new.strip())

# 2. Update fetchDebug javascript
fetch_debug_old = """
    function fetchDebug() {
        if (!document.getElementById('tab-debug').classList.contains('active')) return;
        fetch('/api/debug').then(r => r.json()).then(data => {
            if (data.raw) {
                document.getElementById('debug_out').innerText = data.raw;
            }
        });
    }
"""
fetch_debug_new = """
    function stripAnsi(str) {
        return str.replace(/\\x1b\\[[0-9;]*m/g, '');
    }

    function fetchDebug() {
        if (!document.getElementById('tab-debug').classList.contains('active')) return;
        fetch('/api/debug').then(r => r.json()).then(data => {
            if (data.raw) {
                document.getElementById('debug_out').innerText = data.raw;
            }
            if (data.trace) {
                document.getElementById('trace_out').innerText = stripAnsi(data.trace);
                const to = document.getElementById('trace_out');
                to.scrollTop = to.scrollHeight;
            }
        });
    }
"""
html = html.replace(fetch_debug_old.strip(), fetch_debug_new.strip())

with codecs.open('components/web_server/html/index.html', 'w', 'utf-8') as f:
    f.write(html)
