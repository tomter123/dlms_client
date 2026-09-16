import re
with open('components/web_server/html/index.html', 'r') as f:
    html = f.read()

# Make it completely new layout with tabs
new_html = """<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>DLMS Device Configurator & Dashboard</title>
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
    <style>
        body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background: #f0f2f5; color: #333; margin: 0; padding: 0; }
        .header { background: #0056b3; color: white; padding: 15px 20px; font-size: 20px; font-weight: bold; display: flex; justify-content: space-between; }
        .tabs { display: flex; background: #004494; }
        .tab { padding: 12px 20px; color: white; cursor: pointer; font-weight: bold; }
        .tab:hover { background: #003377; }
        .tab.active { background: #f0f2f5; color: #0056b3; }
        .container { max-width: 800px; margin: 30px auto; padding: 20px; background: white; border-radius: 8px; box-shadow: 0 4px 6px rgba(0,0,0,0.1); }
        .tab-content { display: none; }
        .tab-content.active { display: block; }
        
        /* Form Styles */
        .section { margin-bottom: 25px; padding-bottom: 15px; border-bottom: 1px solid #eee; }
        .section h3 { margin-top: 0; color: #0056b3; font-size: 18px; }
        .form-group { margin-bottom: 15px; }
        .form-row { display: flex; gap: 10px; }
        .form-row .form-group { flex: 1; }
        label { display: block; font-weight: bold; margin-bottom: 5px; font-size: 14px; }
        input[type="text"], input[type="password"], input[type="number"], select { width: 100%; padding: 10px; border: 1px solid #ccc; border-radius: 4px; box-sizing: border-box; font-size: 14px; }
        .checkbox-group { display: flex; align-items: center; }
        .checkbox-group input { width: auto; margin-right: 10px; }
        button { background: #28a745; color: white; border: none; padding: 12px 20px; font-size: 16px; border-radius: 4px; cursor: pointer; width: 100%; font-weight: bold; }
        button:hover { background: #218838; }
        .hidden { display: none; }
        
        /* Debug Styles */
        #debug_out { background: #222; color: #0f0; padding: 15px; border-radius: 4px; overflow-y: scroll; max-height: 400px; font-family: monospace; white-space: pre-wrap; word-break: break-all; }
        
        /* Dashboard Styles */
        .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 15px; margin-bottom: 20px; }
        .card { background: #f8f9fa; border: 1px solid #ddd; padding: 15px; border-radius: 6px; text-align: center; }
        .card h4 { margin: 0 0 10px 0; color: #555; font-size: 14px; }
        .card .value { font-size: 24px; font-weight: bold; color: #0056b3; }
    </style>
</head>
<body>

<div class="header">
    <span>DLMS Smart Gateway</span>
</div>

<div class="tabs">
    <div class="tab active" onclick="switchTab('config')">Configuration</div>
    <div class="tab" onclick="switchTab('dashboard')">Dashboard</div>
    <div class="tab" onclick="switchTab('debug')">Debugging</div>
</div>

<div class="container">
    
    <!-- CONFIGURATION TAB -->
    <div id="tab-config" class="tab-content active">
        <form action="/save" method="POST">
            <!-- Network Settings -->
            <div class="section">
                <h3>1. Network Configuration</h3>
                <div class="form-row">
                    <div class="form-group">
                        <label>Wi-Fi SSID</label>
                        <input type="text" name="ssid" placeholder="Enter Wi-Fi Network Name">
                    </div>
                    <div class="form-group">
                        <label>Wi-Fi Password</label>
                        <input type="password" name="pass" placeholder="Enter Wi-Fi Password">
                    </div>
                </div>
            </div>

            <!-- MQTT Settings -->
            <div class="section">
                <h3>2. Data Export (MQTT)</h3>
                <div class="form-group checkbox-group">
                    <input type="checkbox" id="mqtt_enable" name="mqtt_enable" onchange="toggleMqtt()">
                    <label for="mqtt_enable" style="margin:0;">Enable MQTT Publishing</label>
                </div>
                <div id="mqtt_settings" class="hidden">
                    <div class="form-row">
                        <div class="form-group">
                            <label>Broker IP / Hostname</label>
                            <input type="text" name="mqtt_host" value="192.168.0.12">
                        </div>
                        <div class="form-group">
                            <label>Broker Port</label>
                            <input type="number" name="mqtt_port" value="1883">
                        </div>
                    </div>
                    <div class="form-row">
                        <div class="form-group">
                            <label>Username (Optional)</label>
                            <input type="text" name="mqtt_user">
                        </div>
                        <div class="form-group">
                            <label>Password (Optional)</label>
                            <input type="password" name="mqtt_pass">
                        </div>
                    </div>
                </div>
            </div>

            <!-- Meter Settings -->
            <div class="section">
                <h3>3. Meter Configuration</h3>
                <div class="form-group">
                    <label>Meter Category / Protocol</label>
                    <select id="meter_type" name="meter" onchange="toggleMeterSettings()">
                        <option value="e570">Landis+Gyr E570 (DLMS RS-485)</option>
                        <option value="e450" selected>Landis+Gyr E450 (M-Bus Push)</option>
                        <option value="mbus">Generic M-Bus (Raw)</option>
                    </select>
                </div>

                <!-- E570 Settings -->
                <div id="settings_e570" class="hidden">
                    <div class="form-row">
                        <div class="form-group">
                            <label>Baud Rate</label>
                            <select name="dlms_baud">
                                <option value="9600" selected>9600</option>
                                <option value="19200">19200</option>
                            </select>
                        </div>
                        <div class="form-group">
                            <label>Client Address</label>
                            <input type="number" name="dlms_client" value="32">
                        </div>
                    </div>
                    <div class="form-row">
                        <div class="form-group">
                            <label>Server Logical Address</label>
                            <input type="number" name="dlms_server_logical" value="1">
                        </div>
                        <div class="form-group">
                            <label>Server Physical Address</label>
                            <input type="number" name="dlms_server_physical" value="17">
                        </div>
                    </div>
                    <div class="form-group">
                        <label>Authentication Level</label>
                        <select id="dlms_auth" name="dlms_auth" onchange="toggleAuth()">
                            <option value="none">None</option>
                            <option value="low" selected>Low (Password)</option>
                            <option value="high">High (Keys)</option>
                        </select>
                    </div>
                    <div class="form-group" id="auth_low">
                        <label>Password</label>
                        <input type="password" name="dlms_pass">
                    </div>
                </div>

                <!-- E450 Settings -->
                <div id="settings_e450">
                    <div class="form-row">
                        <div class="form-group">
                            <label>Baud Rate</label>
                            <select name="e450_baud">
                                <option value="2400" selected>2400</option>
                                <option value="9600">9600</option>
                            </select>
                        </div>
                        <div class="form-group">
                            <label>Data Bits</label>
                            <select name="e450_databits">
                                <option value="8" selected>8</option>
                                <option value="7">7</option>
                            </select>
                        </div>
                    </div>
                    <div class="form-row">
                        <div class="form-group">
                            <label>Parity</label>
                            <select name="e450_parity">
                                <option value="none">None</option>
                                <option value="even" selected>Even</option>
                                <option value="odd">Odd</option>
                            </select>
                        </div>
                        <div class="form-group">
                            <label>Stop Bits</label>
                            <select name="e450_stopbits">
                                <option value="1" selected>1</option>
                                <option value="2">2</option>
                            </select>
                        </div>
                    </div>
                </div>

                <!-- M-Bus Settings -->
                <div id="settings_mbus" class="hidden">
                    <div class="form-row">
                        <div class="form-group">
                            <label>Baud Rate</label>
                            <select name="mbus_baud">
                                <option value="2400" selected>2400</option>
                                <option value="9600">9600</option>
                            </select>
                        </div>
                        <div class="form-group">
                            <label>Meter Address</label>
                            <input type="number" name="mbus_addr" value="254">
                        </div>
                    </div>
                    <div class="form-row">
                        <div class="form-group">
                            <label>Data Bits</label>
                            <select name="mbus_databits">
                                <option value="8" selected>8</option>
                            </select>
                        </div>
                        <div class="form-group">
                            <label>Parity</label>
                            <select name="mbus_parity">
                                <option value="even" selected>Even</option>
                                <option value="none">None</option>
                            </select>
                        </div>
                        <div class="form-group">
                            <label>Stop Bits</label>
                            <select name="mbus_stopbits">
                                <option value="1" selected>1</option>
                            </select>
                        </div>
                    </div>
                </div>
            </div>

            <button type="submit">Save & Reboot</button>
        </form>
    </div>
    
    <!-- DASHBOARD TAB -->
    <div id="tab-dashboard" class="tab-content">
        <h3>Live Meter Readings</h3>
        <div class="grid" id="dashboard_grid">
            <div class="card"><h4>Loading data...</h4><div class="value">--</div></div>
        </div>
        
        <h3 style="margin-top: 30px;">Power Graph</h3>
        <canvas id="powerChart" height="100"></canvas>
    </div>
    
    <!-- DEBUGGING TAB -->
    <div id="tab-debug" class="tab-content">
        <h3>Raw Serial Debug (Hex)</h3>
        <div style="margin-bottom: 10px;">
            <button onclick="clearDebug()" style="width:auto; background:#dc3545; padding:8px 15px;">Clear Buffer</button>
            <span style="font-size: 12px; margin-left: 10px;">Auto-refreshes every 2 seconds</span>
        </div>
        <div id="debug_out">Waiting for data...</div>
    </div>

</div>

<script>
    // Tab Switching
    function switchTab(tabId) {
        document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
        document.querySelectorAll('.tab-content').forEach(c => c.classList.remove('active'));
        
        document.querySelector(.tab[onclick="switchTab('')"]).classList.add('active');
        document.getElementById('tab-' + tabId).classList.add('active');
    }

    // Config Toggles
    function toggleMqtt() {
        const enabled = document.getElementById('mqtt_enable').checked;
        document.getElementById('mqtt_settings').style.display = enabled ? 'block' : 'none';
    }

    function toggleMeterSettings() {
        const type = document.getElementById('meter_type').value;
        document.getElementById('settings_e570').style.display = (type === 'e570') ? 'block' : 'none';
        document.getElementById('settings_e450').style.display = (type === 'e450') ? 'block' : 'none';
        document.getElementById('settings_mbus').style.display = (type === 'mbus') ? 'block' : 'none';
    }

    function toggleAuth() {
        const auth = document.getElementById('dlms_auth').value;
        document.getElementById('auth_low').style.display = (auth === 'low') ? 'block' : 'none';
    }

    window.onload = function() {
        toggleMqtt();
        toggleMeterSettings();
        toggleAuth();
        initChart();
        setInterval(fetchDashboard, 5000);
        setInterval(fetchDebug, 2000);
    };

    // Dashboard Fetch
    let chart;
    let chartData = [];
    let chartLabels = [];
    
    function initChart() {
        const ctx = document.getElementById('powerChart').getContext('2d');
        chart = new Chart(ctx, {
            type: 'line',
            data: {
                labels: chartLabels,
                datasets: [{
                    label: 'Active Power (W)',
                    data: chartData,
                    borderColor: '#0056b3',
                    tension: 0.1,
                    fill: false
                }]
            },
            options: { responsive: true, animation: false }
        });
    }

    function fetchDashboard() {
        if (!document.getElementById('tab-dashboard').classList.contains('active')) return;
        fetch('/api/data').then(r => r.json()).then(data => {
            const grid = document.getElementById('dashboard_grid');
            grid.innerHTML = '';
            
            // Render Cards
            for (const [key, val] of Object.entries(data)) {
                if (key === 'uptime_ms') continue;
                if (key === 'power') {
                    // Update graph
                    const now = new Date();
                    chartLabels.push(now.getHours() + ':' + String(now.getMinutes()).padStart(2, '0') + ':' + String(now.getSeconds()).padStart(2, '0'));
                    chartData.push(val);
                    if (chartLabels.length > 20) { chartLabels.shift(); chartData.shift(); }
                    chart.update();
                }
                const card = document.createElement('div');
                card.className = 'card';
                card.innerHTML = <h4></h4><div class="value"></div>;
                grid.appendChild(card);
            }
        }).catch(e => console.log('Fetch error'));
    }

    // Debug Fetch
    function fetchDebug() {
        if (!document.getElementById('tab-debug').classList.contains('active')) return;
        fetch('/api/debug').then(r => r.json()).then(data => {
            if (data.hex) {
                document.getElementById('debug_out').textContent = data.hex;
                // scroll to bottom
                const out = document.getElementById('debug_out');
                out.scrollTop = out.scrollHeight;
            }
        }).catch(e => console.log('Fetch error'));
    }

    function clearDebug() {
        fetch('/api/debug?clear=1').then(() => {
            document.getElementById('debug_out').textContent = 'Cleared.';
        });
    }
</script>

</body>
</html>
"""

with open('components/web_server/html/index.html', 'w') as f:
    f.write(new_html)
