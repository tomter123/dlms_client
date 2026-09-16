import codecs

with codecs.open('components/web_server/html/index.html', 'r', 'utf-8') as f:
    html = f.read()

# 1. Update Chart.js logic in HTML
charts_html = """
        <div style="display: flex; gap: 20px; flex-wrap: wrap;">
            <div style="flex: 1; min-width: 300px;">
                <h3 style="margin-top: 30px;">Power Graph</h3>
                <canvas id="powerChart" height="150"></canvas>
            </div>
            <div style="flex: 1; min-width: 300px;">
                <h3 style="margin-top: 30px;">Voltage Graph</h3>
                <canvas id="voltageChart" height="150"></canvas>
            </div>
            <div style="flex: 1; min-width: 300px;">
                <h3 style="margin-top: 30px;">Current Graph</h3>
                <canvas id="currentChart" height="150"></canvas>
            </div>
        </div>
"""
import re
html = re.sub(r'<h3 style="margin-top: 30px;">Power Graph</h3>.*?<canvas id="powerChart" height="100"></canvas>', charts_html.strip(), html, flags=re.DOTALL)

# Update chart initialization and data fetching
chart_js = """
    let pChart, vChart, cChart;
    let timeLabels = [];
    let pData = { active: [], reactive: [] };
    let vData = { l1: [], l2: [], l3: [] };
    let cData = { l1: [], l2: [], l3: [] };
    
    function initChart() {
        const pCtx = document.getElementById('powerChart').getContext('2d');
        pChart = new Chart(pCtx, {
            type: 'line',
            data: { labels: timeLabels, datasets: [
                { label: 'Active (+A)', data: pData.active, borderColor: '#0056b3', tension: 0.1 },
                { label: 'Reactive (+R)', data: pData.reactive, borderColor: '#ff8c00', tension: 0.1 }
            ]},
            options: { responsive: true, animation: false, scales: { y: { beginAtZero: true } } }
        });

        const vCtx = document.getElementById('voltageChart').getContext('2d');
        vChart = new Chart(vCtx, {
            type: 'line',
            data: { labels: timeLabels, datasets: [
                { label: 'L1', data: vData.l1, borderColor: '#dc3545', tension: 0.1 },
                { label: 'L2', data: vData.l2, borderColor: '#28a745', tension: 0.1 },
                { label: 'L3', data: vData.l3, borderColor: '#ffc107', tension: 0.1 }
            ]},
            options: { responsive: true, animation: false, scales: { y: { min: 200, max: 250 } } }
        });

        const cCtx = document.getElementById('currentChart').getContext('2d');
        cChart = new Chart(cCtx, {
            type: 'line',
            data: { labels: timeLabels, datasets: [
                { label: 'L1', data: cData.l1, borderColor: '#dc3545', tension: 0.1 },
                { label: 'L2', data: cData.l2, borderColor: '#28a745', tension: 0.1 },
                { label: 'L3', data: cData.l3, borderColor: '#ffc107', tension: 0.1 }
            ]},
            options: { responsive: true, animation: false, scales: { y: { beginAtZero: true } } }
        });
    }

    function fetchDashboard() {
        if (!document.getElementById('tab-dashboard').classList.contains('active')) return;
        fetch('/api/data').then(r => r.json()).then(data => {
            const grid = document.getElementById('dashboard_grid');
            grid.innerHTML = '';
            
            const now = new Date();
            timeLabels.push(now.getHours() + ':' + String(now.getMinutes()).padStart(2, '0') + ':' + String(now.getSeconds()).padStart(2, '0'));
            if (timeLabels.length > 20) timeLabels.shift();

            // Track if we got any data for charts
            let actP = null, reactP = null;
            let v1 = null, v2 = null, v3 = null;
            let c1 = null, c2 = null, c3 = null;

            for (const [key, val] of Object.entries(data)) {
                if (key === 'uptime_s') continue;
                
                // Map OBIS to variables
                if (key === '1_0_1_7_0_255') actP = val;
                if (key === '1_0_3_7_0_255') reactP = val;
                if (key === '1_0_32_7_0_255') v1 = val;
                if (key === '1_0_52_7_0_255') v2 = val;
                if (key === '1_0_72_7_0_255') v3 = val;
                if (key === '1_0_31_7_0_255') c1 = val;
                if (key === '1_0_51_7_0_255') c2 = val;
                if (key === '1_0_71_7_0_255') c3 = val;
                
                const card = document.createElement('div');
                card.className = 'card';
                card.innerHTML = '<h4>' + key + '</h4><div class="value">' + val + '</div>';
                grid.appendChild(card);
            }
            
            // Push values (or null if missing) to keep sync
            if (pData.active.length >= 20) pData.active.shift();
            if (pData.reactive.length >= 20) pData.reactive.shift();
            pData.active.push(actP !== null ? actP : (pData.active.length ? pData.active[pData.active.length-1] : 0));
            pData.reactive.push(reactP !== null ? reactP : (pData.reactive.length ? pData.reactive[pData.reactive.length-1] : 0));
            
            if (vData.l1.length >= 20) vData.l1.shift();
            if (vData.l2.length >= 20) vData.l2.shift();
            if (vData.l3.length >= 20) vData.l3.shift();
            vData.l1.push(v1 !== null ? v1 : (vData.l1.length ? vData.l1[vData.l1.length-1] : 230));
            vData.l2.push(v2 !== null ? v2 : (vData.l2.length ? vData.l2[vData.l2.length-1] : 230));
            vData.l3.push(v3 !== null ? v3 : (vData.l3.length ? vData.l3[vData.l3.length-1] : 230));

            if (cData.l1.length >= 20) cData.l1.shift();
            if (cData.l2.length >= 20) cData.l2.shift();
            if (cData.l3.length >= 20) cData.l3.shift();
            cData.l1.push(c1 !== null ? c1 : (cData.l1.length ? cData.l1[cData.l1.length-1] : 0));
            cData.l2.push(c2 !== null ? c2 : (cData.l2.length ? cData.l2[cData.l2.length-1] : 0));
            cData.l3.push(c3 !== null ? c3 : (cData.l3.length ? cData.l3[cData.l3.length-1] : 0));

            pChart.update();
            vChart.update();
            cChart.update();

        }).catch(e => console.log('Fetch error'));
    }
"""

html = re.sub(r'let chart;.*?\}\)\.catch\(e => console\.log\(\'Fetch error\'\)\);\n    \}', chart_js.strip(), html, flags=re.DOTALL)

# Add MQTT template editor HTML
template_editor = """
                    <div class="form-row">
                        <div class="form-group" style="flex:1;">
                            <label>MQTT Payload Template (JSON)</label>
                            <p style="font-size:12px; color:#666; margin-top:0;">Use OBIS codes in braces e.g., <code>{1.0.1.7.0.255}</code> to inject values dynamically.</p>
                            <textarea id="mqtt_tpl" name="mqtt_tpl" rows="10" style="width:100%; font-family:monospace; font-size:12px; padding:10px; border:1px solid #ccc; border-radius:4px;"></textarea>
                        </div>
                    </div>
"""
html = html.replace('<!-- MQTT Settings -->\n            <div class="section">\n                <h3>2. Data Export (MQTT)</h3>', '<!-- MQTT Settings -->\n            <div class="section">\n                <h3>2. Data Export (MQTT)</h3>' + template_editor)

# Add init logic to load config
load_config = """
        fetch('/api/config').then(r => r.json()).then(data => {
            if(data.mqtt_tpl) document.getElementById('mqtt_tpl').value = data.mqtt_tpl;
            else document.getElementById('mqtt_tpl').value = {\n  "device_id": "{0.0.42.0.0.255}",\n  "power_active": {1.0.1.7.0.255},\n  "power_reactive": {1.0.3.7.0.255},\n  "voltage_l1": {1.0.32.7.0.255},\n  "voltage_l2": {1.0.52.7.0.255},\n  "voltage_l3": {1.0.72.7.0.255},\n  "current_l1": {1.0.31.7.0.255},\n  "current_l2": {1.0.51.7.0.255},\n  "current_l3": {1.0.71.7.0.255}\n};
        }).catch(e=>{});
"""
html = html.replace('toggleAuth();\n        initChart();', 'toggleAuth();\n        initChart();\n' + load_config)

with codecs.open('components/web_server/html/index.html', 'w', 'utf-8') as f:
    f.write(html)
