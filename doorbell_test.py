"""
ESP32 Doorbell Tester - Lokale Web Server
==========================================

Dit script draait op een lokale server (Raspberry Pi, Android phone, etc.)
en stuurt UDP pakketten naar de deurbel ontvanger.

Gebruik:
    python doorbell_test.py

Toegankelijk via: http://[uw-ip-adres]:5000
"""

import socket
import os
from flask import Flask, render_template_string, jsonify, request

app = Flask(__name__)

# ============================================
# CONFIGURATIE - AANPASSEN INDIEN NODIG
# ============================================

# IP adres van de deurbel ontvanger
TARGET_IP = "192.168.170.202"

# Poort van de deurbel ontvanger
TARGET_PORT = 4210

# Server poort (alleen aanpassen als poort 5000 bezet is)
SERVER_PORT = 5000

# ============================================
# HTML TEMPLATE (INGLEDEED)
# ============================================

HTML_TEMPLATE = """
<!DOCTYPE html>
<html lang="nl">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <title>ESP32 Deurbel Tester</title>
    <style>
        * {
            box-sizing: border-box;
            margin: 0;
            padding: 0;
        }
        
        body {
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
            background: linear-gradient(135deg, #1a1a2e 0%, #16213e 100%);
            color: #ffffff;
            min-height: 100vh;
            display: flex;
            flex-direction: column;
            align-items: center;
            padding: 20px;
        }
        
        .container {
            width: 100%;
            max-width: 400px;
            text-align: center;
        }
        
        h1 {
            font-size: 28px;
            margin-bottom: 10px;
            text-shadow: 0 2px 4px rgba(0,0,0,0.3);
        }
        
        .subtitle {
            color: #a0a0a0;
            font-size: 14px;
            margin-bottom: 30px;
        }
        
        .status-bar {
            background: rgba(255,255,255,0.1);
            border-radius: 10px;
            padding: 15px;
            margin-bottom: 30px;
            font-size: 13px;
            color: #888;
        }
        
        .status-bar span {
            color: #4ade80;
            font-weight: bold;
        }
        
        .button-container {
            display: flex;
            flex-direction: column;
            gap: 20px;
        }
        
        .doorbell-btn {
            border: none;
            padding: 30px 20px;
            font-size: 22px;
            font-weight: bold;
            border-radius: 20px;
            cursor: pointer;
            transition: all 0.15s ease;
            color: white;
            box-shadow: 0 6px 20px rgba(0,0,0,0.3);
            position: relative;
            overflow: hidden;
        }
        
        .doorbell-btn::before {
            content: '';
            position: absolute;
            top: 0;
            left: -100%;
            width: 100%;
            height: 100%;
            background: linear-gradient(90deg, transparent, rgba(255,255,255,0.2), transparent);
            transition: left 0.5s;
        }
        
        .doorbell-btn:active::before {
            left: 100%;
        }
        
        .doorbell-btn:active {
            transform: scale(0.96);
            box-shadow: 0 2px 10px rgba(0,0,0,0.3);
        }
        
        .btn-ring {
            background: linear-gradient(135deg, #3b82f6 0%, #1d4ed8 100%);
            border-bottom: 5px solid #1e40af;
        }
        
        .btn-ring:active {
            border-bottom: 2px solid #1e40af;
            transform: translateY(3px);
        }
        
        .btn-pong {
            background: linear-gradient(135deg, #10b981 0%, #059669 100%);
            border-bottom: 5px solid #047857;
        }
        
        .btn-pong:active {
            border-bottom: 2px solid #047857;
            transform: translateY(3px);
        }
        
        .btn-icon {
            font-size: 32px;
            display: block;
            margin-bottom: 8px;
        }
        
        .btn-text {
            display: block;
        }
        
        .log-area {
            margin-top: 30px;
            background: rgba(0,0,0,0.3);
            border-radius: 10px;
            padding: 15px;
            min-height: 60px;
            display: flex;
            align-items: center;
            justify-content: center;
        }
        
        .log-message {
            font-size: 16px;
            font-weight: 500;
            opacity: 0;
            transition: opacity 0.3s;
        }
        
        .log-message.show {
            opacity: 1;
        }
        
        .log-message.success {
            color: #4ade80;
        }
        
        .log-message.error {
            color: #f87171;
        }
        
        .log-message.sending {
            color: #fbbf24;
        }
        
        .footer {
            margin-top: auto;
            padding-top: 30px;
            color: #555;
            font-size: 12px;
        }
        
        /* Pulse animation for status LED */
        @keyframes pulse {
            0%, 100% { opacity: 1; }
            50% { opacity: 0.5; }
        }
        
        .led {
            display: inline-block;
            width: 10px;
            height: 10px;
            border-radius: 50%;
            background: #4ade80;
            margin-right: 8px;
            animation: pulse 2s infinite;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>🔔 Deurbel Tester</h1>
        <p class="subtitle">ESP32 Remote Control</p>
        
        <div class="status-bar">
            <div><span class="led"></span>Server Actief</div>
            <div style="margin-top: 5px;">Doel: {{ target_ip }}:{{ target_port }}</div>
        </div>
        
        <div class="button-container">
            <button class="doorbell-btn btn-ring" onclick="triggerMelody('RING')">
                <span class="btn-icon">🎵</span>
                <span class="btn-text">TEST RING</span>
            </button>
            
            <button class="doorbell-btn btn-pong" onclick="triggerMelody('PONG')">
                <span class="btn-icon">🔊</span>
                <span class="btn-text">TEST PONG</span>
            </button>
        </div>
        
        <div class="log-area">
            <span id="logMessage" class="log-message">Klaar om te testen...</span>
        </div>
        
        <div class="footer">
            <p>WiFi: {{ wifi_name }}</p>
            <p>IP: {{ local_ip }}</p>
        </div>
    </div>

    <script>
        let isSending = false;
        
        function triggerMelody(melody) {
            if (isSending) return;
            
            const logEl = document.getElementById('logMessage');
            isSending = true;
            
            // Show sending status
            logEl.className = 'log-message show sending';
            logEl.textContent = 'Versturen ' + melody + '...';
            
            // Send request to server
            fetch('/trigger/' + melody)
                .then(response => response.json())
                .then(data => {
                    if (data.success) {
                        logEl.className = 'log-message show success';
                        logEl.textContent = '✓ ' + melody + ' verzonden!';
                        
                        // Haptic feedback on Android
                        if (navigator.vibrate) {
                            navigator.vibrate(50);
                        }
                    } else {
                        logEl.className = 'log-message show error';
                        logEl.textContent = '✗ Fout: ' + data.error;
                    }
                })
                .catch(err => {
                    logEl.className = 'log-message show error';
                    logEl.textContent = '✗ Verbinding mislukt';
                })
                .finally(() => {
                    setTimeout(() => {
                        logEl.classList.remove('show');
                        isSending = false;
                    }, 2000);
                });
        }
    </script>
</body>
</html>
"""

def get_local_ip():
    """Haal het lokale IP adres van de server op"""
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except Exception:
        return "127.0.0.1"

def get_wifi_name():
    """Probeer de WiFi netwerk naam op te halen (alleen Linux)"""
    try:
        if os.path.exists('/sys/class/net/wlan0/address'):
            return "WiFi Netwerk"
    except:
        pass
    return "Lokaal Netwerk"

@app.route('/')
def index():
    """Toon de hoofdpagina"""
    return render_template_string(
        HTML_TEMPLATE,
        target_ip=TARGET_IP,
        target_port=TARGET_PORT,
        local_ip=get_local_ip(),
        wifi_name=get_wifi_name()
    )

@app.route('/trigger/<melody>')
def trigger(melody):
    """Verstuur UDP pakket naar de deurbel"""
    # Valideer invoer
    if melody not in ['RING', 'PONG']:
        return jsonify(success=False, error="Ongeldige melodie")
    
    try:
        # Maak UDP socket
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        
        # Verstuur het pakket
        sock.sendto(melody.encode('utf-8'), (TARGET_IP, TARGET_PORT))
        sock.close()
        
        print(f"✓ {melody} verzonden naar {TARGET_IP}:{TARGET_PORT}")
        
        return jsonify(success=True, message=f"{melody} verzonden")
        
    except Exception as e:
        print(f"✗ Fout bij verzenden: {e}")
        return jsonify(success=False, error=str(e))

if __name__ == '__main__':
    local_ip = get_local_ip()
    
    print("=" * 50)
    print("   ESP32 Deurbel Tester - Server Gestart")
    print("=" * 50)
    print()
    print(f"Toegankelijk via:")
    print(f"   http://localhost:{SERVER_PORT}")
    print(f"   http://{local_ip}:{SERVER_PORT}")
    print()
    print(f"Stuurt UDP naar: {TARGET_IP}:{TARGET_PORT}")
    print()
    print("Druk Ctrl+C om te stoppen")
    print("=" * 50)
    
    # Start de server op alle beschikbare netwerk interfaces
    app.run(host='0.0.0.0', port=SERVER_PORT, debug=False)
