import codecs

fixed_code = """
    function switchTab(tabId) {
        document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
        document.querySelectorAll('.tab-content').forEach(c => c.classList.remove('active'));
        
        document.querySelector('.tab[onclick="switchTab(\\'' + tabId + '\\')"]').classList.add('active');
        document.getElementById('tab-' + tabId).classList.add('active');
    }
"""

fixed_card = """
                const card = document.createElement('div');
                card.className = 'card';
                card.innerHTML = '<h4>' + key + '</h4><div class="value">' + val + '</div>';
                grid.appendChild(card);
"""

with codecs.open('components/web_server/html/index.html', 'r', 'utf-8') as f:
    text = f.read()

import re
text = re.sub(r'function switchTab\(tabId\) \{.*?\}', fixed_code.strip(), text, flags=re.DOTALL)

text = re.sub(r'const card = document\.createElement.*?grid\.appendChild\(card\);', fixed_card.strip(), text, flags=re.DOTALL)

with codecs.open('components/web_server/html/index.html', 'w', 'utf-8') as f:
    f.write(text)
