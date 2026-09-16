import codecs
with open('components/web_server/html/index.html', 'r', encoding='utf-8') as f:
    c = f.read()

fetch_old = """        fetch('/api/config').then(r => r.json()).then(data => {
            if(data.mqtt_tpl) document.getElementById('mqtt_tpl').value = data.mqtt_tpl;
            else document.getElementById('mqtt_tpl').value = '{\\n  "device_id": "{0.0.42.0.0.255}",\\n  "power_active": {1.0.1.7.0.255},\\n  "power_reactive": {1.0.3.7.0.255},\\n  "voltage_l1": {1.0.32.7.0.255},\\n  "voltage_l2": {1.0.52.7.0.255},\\n  "voltage_l3": {1.0.72.7.0.255},\\n  "current_l1": {1.0.31.7.0.255},\\n  "current_l2": {1.0.51.7.0.255},\\n  "current_l3": {1.0.71.7.0.255}\\n}';
        }).catch(e=>{});"""

fetch_new = """        fetch('/api/config').then(r => r.json()).then(data => {
            if(data.mqtt_tpl) document.getElementById('mqtt_tpl').value = data.mqtt_tpl;
            else document.getElementById('mqtt_tpl').value = '{\\n  "device_id": "{0.0.42.0.0.255}",\\n  "power_active": {1.0.1.7.0.255},\\n  "power_reactive": {1.0.3.7.0.255},\\n  "voltage_l1": {1.0.32.7.0.255},\\n  "voltage_l2": {1.0.52.7.0.255},\\n  "voltage_l3": {1.0.72.7.0.255},\\n  "current_l1": {1.0.31.7.0.255},\\n  "current_l2": {1.0.51.7.0.255},\\n  "current_l3": {1.0.71.7.0.255}\\n}';
            
            if(data.meter_type) document.getElementById('meter_type').value = data.meter_type;
            if(data.mqtt_en !== undefined) document.getElementById('mqtt_enable').checked = (data.mqtt_en === 1);
            if(data.mqtt_uri) document.querySelector('[name="mqtt_uri"]').value = data.mqtt_uri;
            if(data.mqtt_topic) document.querySelector('[name="mqtt_topic"]').value = data.mqtt_topic;
            if(data.mqtt_user) document.querySelector('[name="mqtt_user"]').value = data.mqtt_user;
            if(data.mqtt_pass) document.querySelector('[name="mqtt_pass"]').value = data.mqtt_pass;
            if(data.auth) document.querySelector('[name="auth"]').value = data.auth;
            if(data.pass) document.querySelector('[name="pass"]').value = data.pass;
            if(data.parity) document.querySelector('[name="parity"]').value = data.parity;
            if(data.baud) document.querySelector('[name="baud"]').value = data.baud;
            if(data.client_addr) document.querySelector('[name="client_addr"]').value = data.client_addr;
            if(data.server_logical) document.querySelector('[name="server_logical"]').value = data.server_logical;
            if(data.server_physical) document.querySelector('[name="server_physical"]').value = data.server_physical;
            
            toggleMqtt();
            toggleMeterSettings();
            toggleAuth();
        }).catch(e=>{});"""

c = c.replace(fetch_old, fetch_new)

with open('components/web_server/html/index.html', 'w', encoding='utf-8') as f:
    f.write(c)

