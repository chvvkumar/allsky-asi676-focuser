/*
 * Web Interface for XIAO ESP32-S3 Lens Focus Controller v2.0
 * Enhanced with WebSocket Support and Error Feedback
 */

#ifndef WEB_INTERFACE_H
#define WEB_INTERFACE_H

#include <Arduino.h>

const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <title>Lens Controller v2.0</title>
    <style>
        @import url('https://fonts.googleapis.com/css2?family=JetBrains+Mono:wght@400;700&display=swap');
        
        :root {
            --bg: #000000;
            --surface: #0a0a0a;
            --surface-hover: #1f1f1f;
            --primary: #be123c;
            --text-main: #e2e8f0;
            --text-muted: #94a3b8;
            --danger: #7f1d1d;
            --success: #22c55e;
            --warning: #fbbf24;
            --radius: 12px;
            --shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.5);
        }

        * { box-sizing: border-box; -webkit-tap-highlight-color: transparent; }
        
        body {
            font-family: 'JetBrains Mono', -apple-system, monospace;
            background-color: var(--bg);
            color: var(--text-main);
            margin: 0;
            display: flex;
            justify-content: center;
            min-height: 100vh;
            background-image: linear-gradient(rgba(51, 65, 85, 0.1) 1px, transparent 1px),
            linear-gradient(90deg, rgba(51, 65, 85, 0.1) 1px, transparent 1px);
            background-size: 20px 20px;
        }
        
        .app-container {
            width: 100%;
            max-width: 900px;
            padding: 16px;
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 20px;
            align-items: start;
        }

        @media (max-width: 768px) {
            .app-container {
                grid-template-columns: 1fr;
            }
        }
        
        .col-wrapper {
            display: flex;
            flex-direction: column;
            gap: 16px;
        }

        .card {
            background: var(--surface);
            border-radius: var(--radius);
            padding: 20px;
            box-shadow: var(--shadow);
            border: 1px solid var(--surface-hover);
        }
        
        .btn-stop {
            background-color: var(--danger);
            color: white;
            border: none;
            padding: 20px;
            font-size: 1.2rem;
            font-weight: 700;
            border-radius: var(--radius);
            text-transform: uppercase;
            letter-spacing: 1px;
            cursor: pointer;
            box-shadow: var(--shadow);
            transition: transform 0.1s, opacity 0.1s;
            width: 100%;
            font-family: inherit;
        }
        .btn-stop:active { transform: scale(0.98); opacity: 0.9; }
        
        .graphic-container {
            display: flex;
            justify-content: center;
            align-items: center;
            margin-bottom: 20px;
            background: var(--bg);
            border-radius: 12px;
            padding: 0;
            border: 1px solid var(--surface-hover);
            overflow: hidden;
            width: 100%;
            aspect-ratio: 1 / 1; 
            max-width: 400px;
            margin-left: auto;
            margin-right: auto;
        }
        
        .motor-shaft {
            transform-origin: 150px 110px;
            transition: transform 0.15s cubic-bezier(0.4, 0.0, 0.2, 1);
        }
        
        .status-grid {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 10px;
            margin-bottom: 15px;
        }
        
        .stat-box {
            background: rgba(255, 255, 255, 0.05);
            padding: 10px;
            border-radius: 6px;
            border: 1px solid var(--surface-hover);
        }
        
        .stat-label {
            font-size: 0.7rem;
            color: var(--text-muted);
            text-transform: uppercase;
            margin-bottom: 4px;
        }
        
        .stat-value {
            font-size: 1.2rem;
            font-weight: bold;
            color: var(--primary);
            font-family: monospace;
        }
        
        .status-row {
            display: flex;
            justify-content: space-between;
            border-top: 1px solid var(--surface-hover);
            padding-top: 15px;
            color: var(--text-muted);
            font-size: 0.9rem;
        }
        .status-item { display: flex; align-items: center; gap: 8px; }
        .dot { width: 8px; height: 8px; border-radius: 50%; background: #475569; }
        .dot.active { background: var(--success); box-shadow: 0 0 8px var(--success); }
        .dot.warning { background: var(--warning); box-shadow: 0 0 8px var(--warning); }
        
        .section-title {
            color: var(--text-muted);
            font-size: 0.8rem;
            text-transform: uppercase;
            margin-bottom: 10px;
            text-align: center;
            letter-spacing: 0.5px;
        }
        
        .btn-grid {
            display: grid;
            grid-template-columns: repeat(4, 1fr);
            gap: 8px;
            margin-bottom: 12px;
        }
        
        .btn-ctrl {
            background: var(--surface-hover);
            border: none;
            color: var(--text-main);
            padding: 16px 0;
            border-radius: 8px;
            font-size: 1.1rem;
            font-weight: 600;
            cursor: pointer;
            transition: background 0.1s;
            font-family: inherit;
        }
        .btn-ctrl:active { background: var(--primary); color: #000; }
        
        .slider-container {
            display: flex;
            align-items: center;
            gap: 15px;
            padding: 5px 0;
        }
        input[type="range"] {
            flex: 1;
            height: 6px;
            background: var(--surface-hover);
            border-radius: 3px;
            outline: none;
            -webkit-appearance: none;
        }
        input[type="range"]::-webkit-slider-thumb {
            -webkit-appearance: none;
            width: 20px;
            height: 20px;
            background: var(--primary);
            border-radius: 50%;
            cursor: pointer;
            box-shadow: 0 0 10px rgba(244, 63, 94, 0.4);
        }
        
        .settings-btn {
            background: none;
            border: none;
            color: var(--text-muted);
            width: 100%;
            padding: 15px;
            text-align: left;
            font-size: 0.95rem;
            cursor: pointer;
            display: flex;
            justify-content: space-between;
            font-family: inherit;
        }
        
        .settings-content {
            display: none;
            padding: 0 15px 15px;
        }
        .settings-content.open { display: block; }
        
        .input-group {
            display: flex;
            gap: 10px;
            margin-bottom: 12px;
        }
        
        input[type="number"] {
            background: #0f172a;
            border: 1px solid var(--surface-hover);
            color: var(--text-main);
            padding: 12px;
            border-radius: 6px;
            flex: 1;
            font-size: 1rem;
            font-family: inherit;
        }
        
        .btn-action {
            padding: 0 20px;
            background: var(--surface-hover);
            border: none;
            color: var(--text-main);
            border-radius: 6px;
            cursor: pointer;
            font-weight: 500;
            font-family: inherit;
        }
        
        .gauge-track {
            background: var(--surface-hover);
            height: 12px;
            border-radius: 6px;
            position: relative;
            margin-top: 10px;
            overflow: hidden;
        }
        .gauge-bar {
            background: var(--primary);
            height: 100%;
            position: absolute;
            top: 0;
            transition: all 0.2s;
        }
        .gauge-center-mark {
            position: absolute;
            left: 50%;
            top: 0;
            bottom: 0;
            width: 2px;
            background: rgba(255,255,255,0.3);
            transform: translateX(-50%);
            z-index: 2;
        }
        
        /* Error/Success Toast */
        .toast {
            position: fixed;
            top: 20px;
            right: 20px;
            padding: 15px 20px;
            border-radius: var(--radius);
            background: var(--surface);
            border: 1px solid var(--surface-hover);
            box-shadow: var(--shadow);
            display: none;
            z-index: 1000;
            animation: slideIn 0.3s ease;
        }
        .toast.show { display: block; }
        .toast.error { border-color: var(--danger); color: var(--danger); }
        .toast.success { border-color: var(--success); color: var(--success); }
        .toast.warning { border-color: var(--warning); color: var(--warning); }
        
        @keyframes slideIn {
            from { transform: translateX(100%); opacity: 0; }
            to { transform: translateX(0); opacity: 1; }
        }
        
        /* Connection Status Badge */
        .connection-badge {
            position: fixed;
            top: 10px;
            left: 10px;
            padding: 5px 10px;
            border-radius: 20px;
            font-size: 0.7rem;
            background: var(--surface);
            border: 1px solid var(--surface-hover);
            display: flex;
            align-items: center;
            gap: 5px;
        }
    </style>
</head>
<body>
    <div class="connection-badge">
        <div class="dot" id="wsDot"></div>
        <span id="wsStatus">Connecting...</span>
    </div>
    
    <div class="toast" id="toast"></div>
    
    <div class="app-container">
        <div class="col-wrapper">
            <div class="card">
                <div class="graphic-container">
                    <svg width="100%" height="100%" viewBox="40 10 220 210" preserveAspectRatio="xMidYMid meet" xmlns="http://www.w3.org/2000/svg">
                        <defs>
                            <linearGradient id="metalGradient" x1="0%" y1="0%" x2="100%" y2="100%">
                                <stop offset="0%" style="stop-color:#94a3b8;stop-opacity:1" />
                                <stop offset="50%" style="stop-color:#cbd5e1;stop-opacity:1" />
                                <stop offset="100%" style="stop-color:#94a3b8;stop-opacity:1" />
                            </linearGradient>
                            <radialGradient id="darkCenter" cx="50%" cy="50%" r="50%" fx="50%" fy="50%">
                                <stop offset="0%" style="stop-color:#334155;stop-opacity:1" />
                                <stop offset="100%" style="stop-color:#0f172a;stop-opacity:1" />
                            </radialGradient>
                            <filter id="dropShadow" x="-20%" y="-20%" width="140%" height="140%">
                                <feGaussianBlur in="SourceAlpha" stdDeviation="4"/>
                                <feOffset dx="2" dy="4" result="offsetblur"/>
                                <feComponentTransfer>
                                  <feFuncA type="linear" slope="0.5"/>
                                </feComponentTransfer>
                                <feMerge> 
                                    <feMergeNode/>
                                    <feMergeNode in="SourceGraphic"/> 
                                </feMerge>
                            </filter>
                        </defs>

                        <path d="M90,165 C90,190 70,190 50,190" stroke="var(--danger)" stroke-width="3" fill="none"/>
                        <path d="M96,165 C96,195 70,195 50,195" stroke="#3b82f6" stroke-width="3" fill="none"/>
                        <path d="M102,165 C102,200 70,200 50,200" stroke="#22c55e" stroke-width="3" fill="none"/>
                        <path d="M108,165 C108,205 70,205 50,205" stroke="#000000" stroke-width="3" fill="none"/>
                        <rect x="85" y="160" width="30" height="15" rx="2" fill="#f1f5f9" />
                        <rect x="85" y="45" width="130" height="130" rx="8" fill="url(#metalGradient)" filter="url(#dropShadow)" stroke="#64748b" stroke-width="1"/>
                        <circle cx="98" cy="58" r="4" fill="#475569" />
                        <circle cx="202" cy="58" r="4" fill="#475569" />
                        <circle cx="98" cy="162" r="4" fill="#475569" />
                        <circle cx="202" cy="162" r="4" fill="#475569" />
                        <circle cx="150" cy="110" r="45" fill="url(#darkCenter)" stroke="#334155" stroke-width="2" />
                        <text x="150" y="150" text-anchor="middle" fill="#64748b" font-size="8" font-family="monospace">POSITION</text>
                        <g id="motorShaft" class="motor-shaft">
                            <circle cx="150" cy="110" r="12" fill="#e2e8f0" stroke="#94a3b8" />
                            <path d="M138,105 L162,105" stroke="#94a3b8" stroke-width="1" />
                            <circle cx="150" cy="110" r="30" fill="transparent" stroke="#f43f5e" stroke-width="2" stroke-dasharray="5,5" opacity="0.5"/>
                            <line x1="150" y1="110" x2="150" y2="70" stroke="#f43f5e" stroke-width="3" stroke-linecap="round"/>
                            <circle cx="150" cy="70" r="4" fill="#f43f5e" />
                        </g>
                    </svg>
                </div>

                <div class="status-grid">
                    <div class="stat-box">
                        <div class="stat-label">Position</div>
                        <div class="stat-value" id="posDisp">---</div>
                    </div>
                    <div class="stat-box">
                        <div class="stat-label">Target</div>
                        <div class="stat-value" id="targetDisp">---</div>
                    </div>
                </div>

                <div style="font-size: 0.7rem; color: var(--text-muted); display: flex; justify-content: space-between;">
                    <span id="minLabel">-20k</span>
                    <span>0</span>
                    <span id="maxLabel">+20k</span>
                </div>
                <div class="gauge-track">
                    <div class="gauge-center-mark"></div>
                    <div class="gauge-bar" id="posBar"></div>
                </div>

                <div class="status-row">
                    <div class="status-item">
                        <div class="dot" id="statusDot"></div>
                        <span id="statusText">Idle</span>
                    </div>
                </div>
            </div>
        </div>

        <div class="col-wrapper">
            <button class="btn-stop" onclick="stopMotor()">EMERGENCY STOP</button>
            
            <div class="card">
                <div class="section-title">Focus Adjustment</div>
                
                <div class="btn-grid">
                    <button class="btn-ctrl" onclick="nudge(-10)">&laquo;</button>
                    <button class="btn-ctrl" onclick="nudge(-1)">&lsaquo;</button>
                    <button class="btn-ctrl" onclick="nudge(1)">&rsaquo;</button>
                    <button class="btn-ctrl" onclick="nudge(10)">&raquo;</button>
                </div>
                
                <div class="btn-grid">
                    <button class="btn-ctrl" onclick="nudge(-1000)">-1k</button>
                    <button class="btn-ctrl" onclick="nudge(-100)">-100</button>
                    <button class="btn-ctrl" onclick="nudge(100)">+100</button>
                    <button class="btn-ctrl" onclick="nudge(1000)">+1k</button>
                </div>

                <div style="margin-top: 20px;">
                    <div class="section-title">Speed: <span id="speedVal" style="color:var(--primary)">250</span></div>
                    <div class="slider-container">
                        <span style="font-size:0.8rem">50</span>
                        <input type="range" id="speedSlider" min="50" max="600" step="10" onchange="commitSpeed()" oninput="previewSpeed(this.value)">
                        <span style="font-size:0.8rem">600</span>
                    </div>
                </div>
            </div>

            <div class="card" style="padding: 15px;">
                 <div class="btn-grid" style="grid-template-columns: 1fr 1fr; margin-bottom: 0;">
                    <button class="btn-ctrl" onclick="goToZero()">Go Home (0)</button>
                    <button class="btn-ctrl" style="color:var(--danger);" onclick="setZero()">Set 0 Here</button>
                </div>
            </div>

            <div class="card" style="padding: 0;">
                <button class="settings-btn" onclick="toggleSettings()">
                    <span>Tools & Configuration</span>
                    <span>&#9881;</span>
                </button>
                <div class="settings-content" id="settingsContent">
                    <div class="stat-label">Target Position</div>
                    <div class="input-group">
                        <input type="number" id="gotoInput" placeholder="Go to position">
                        <button class="btn-action" style="background: var(--primary); color: #000;" onclick="goToPos()">GO</button>
                    </div>
                    
                    <div style="border-top:1px solid var(--surface-hover); margin:10px 0;"></div>
                    
                    <div class="stat-label">Max Travel Limit (+/-)</div>
                    <div class="input-group">
                        <input type="number" id="maxInput" placeholder="Limit">
                        <button class="btn-action" onclick="saveCfg('max')">Save</button>
                    </div>
                    
                    <div class="stat-label">Steps Per Rotation</div>
                    <div class="input-group">
                        <input type="number" id="stepsRotInput" placeholder="Steps/Rot">
                        <button class="btn-action" onclick="saveCfg('rot')">Save</button>
                    </div>

                    <div style="border-top:1px solid var(--surface-hover); margin:10px 0;"></div>
                    
                    <button class="btn-action" style="width:100%; padding:15px; margin-top:5px;" onclick="window.location.href='/update'">
                        Update Firmware
                    </button>
                    
                    <button class="btn-action" style="width:100%; padding:15px; margin-top:5px; background: var(--danger); color: #fff;" onclick="rebootDevice()">
                        Reboot Device
                    </button>
                </div>
            </div>
        </div>
    </div>

    <script>
        // WebSocket connection
        let ws = null;
        let wsReconnectInterval = null;
        
        // State
        let state = { pos: 0, target: 0, speed: 250, running: false, stepsPerRot: 4096 };
        
        // Utils
        const $ = id => document.getElementById(id);
        const vib = () => { if(navigator.vibrate) navigator.vibrate(10); };

        // WebSocket Functions
        function connectWebSocket() {
            const wsProtocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
            const wsPort = 81;
            const wsUrl = wsProtocol + '//' + window.location.hostname + ':' + wsPort;
            
            try {
                ws = new WebSocket(wsUrl);
                
                ws.onopen = () => {
                    console.log('WebSocket connected');
                    updateConnectionStatus(true);
                    if(wsReconnectInterval) {
                        clearInterval(wsReconnectInterval);
                        wsReconnectInterval = null;
                    }
                };
                
                ws.onmessage = (event) => {
                    try {
                        const data = JSON.parse(event.data);
                        updateUI(data);
                    } catch(e) {
                        console.error('Parse error:', e);
                    }
                };
                
                ws.onerror = (error) => {
                    console.error('WebSocket error:', error);
                    updateConnectionStatus(false);
                };
                
                ws.onclose = () => {
                    console.log('WebSocket disconnected');
                    updateConnectionStatus(false);
                    // Attempt reconnect
                    if(!wsReconnectInterval) {
                        wsReconnectInterval = setInterval(() => {
                            console.log('Attempting WebSocket reconnect...');
                            connectWebSocket();
                        }, 3000);
                    }
                };
            } catch(e) {
                console.error('WebSocket connection failed:', e);
                updateConnectionStatus(false);
            }
        }
        
        function updateConnectionStatus(connected) {
            const dot = $('wsDot');
            const status = $('wsStatus');
            if(connected) {
                dot.classList.add('active');
                status.textContent = 'Connected';
            } else {
                dot.classList.remove('active');
                status.textContent = 'Disconnected';
            }
        }
        
        // Toast notification
        function showToast(message, type = 'success') {
            const toast = $('toast');
            toast.textContent = message;
            toast.className = 'toast show ' + type;
            setTimeout(() => {
                toast.classList.remove('show');
            }, 3000);
        }

        function toggleSettings() {
            $('settingsContent').classList.toggle('open');
        }

        function updateUI(data) {
            state = data;
            
            $('posDisp').textContent = data.position;
            $('targetDisp').textContent = data.target;
            
            if($('maxLabel')) {
                $('maxLabel').textContent = '+' + (data.maxSteps/1000).toFixed(1) + 'k';
                $('minLabel').textContent = '-' + (data.maxSteps/1000).toFixed(1) + 'k';
            }
            
            // Update bi-directional bar
            const pct = parseFloat(data.percentage);
            const bar = $('posBar');
            if (pct >= 50) {
                bar.style.left = '50%';
                bar.style.width = (pct - 50) + '%';
            } else {
                bar.style.left = pct + '%';
                bar.style.width = (50 - pct) + '%';
            }
            
            // Rotation animation
            if(data.stepsPerRot > 0) {
                const angle = (data.position / data.stepsPerRot) * 360;
                const shaft = $('motorShaft');
                if(shaft) {
                    shaft.style.transform = 'rotate(' + angle + 'deg)';
                }
            }
            
            // Status display
            const dot = $('statusDot');
            const txt = $('statusText');
            
            dot.classList.remove('active', 'warning');
            
            if(data.nearLimit) {
                dot.classList.add('warning');
                txt.textContent = 'Near Limit!';
                txt.style.color = 'var(--warning)';
            } else if (data.running) {
                dot.classList.add('active');
                const stateNames = ['Idle', 'Running', 'Stopped', 'E-Stop'];
                txt.textContent = stateNames[data.state] || 'Moving...';
                txt.style.color = 'var(--success)';
            } else {
                txt.textContent = 'Idle';
                txt.style.color = 'var(--text-muted)';
            }
            
            // Update inputs if not focused
            if (document.activeElement !== $('speedSlider')) {
                $('speedSlider').value = data.speed;
                $('speedVal').textContent = data.speed;
            }
            if (document.activeElement !== $('maxInput')) $('maxInput').value = data.maxSteps;
            if (document.activeElement !== $('stepsRotInput')) $('stepsRotInput').value = data.stepsPerRot;
        }

        // API Calls with error handling
        async function apiCall(endpoint, method = 'GET', body = null, paramName = null) {
            try {
                const options = { method };
                if(body !== null) {
                    // Use provided param name or infer from endpoint
                    const key = paramName || endpoint.split('/').pop();
                    options.body = JSON.stringify({ [key]: body });
                    options.headers = { 'Content-Type': 'application/json' };
                }
                
                const response = await fetch(endpoint, options);
                const data = await response.json();
                
                if(data.status === 'error') {
                    showToast(data.message || 'Error occurred', 'error');
                } else if(data.status === 'warning') {
                    showToast(data.message || 'Warning', 'warning');
                } else if(data.message) {
                    showToast(data.message, 'success');
                }
                
                return data;
            } catch(e) {
                showToast('Connection error', 'error');
                console.error('API call failed:', e);
            }
        }

        function stopMotor() {
            vib();
            apiCall('/api/stop', 'POST');
        }

        function nudge(steps) {
            vib();
            apiCall('/api/nudge', 'POST', steps, 'steps');
        }

        function previewSpeed(val) {
            $('speedVal').textContent = val;
        }

        function commitSpeed() {
            const val = $('speedSlider').value;
            apiCall('/api/speed', 'POST', parseInt(val), 'speed');
        }

        function goToPos() {
            const val = $('gotoInput').value;
            if(!val) return;
            apiCall('/api/position', 'POST', parseInt(val), 'position');
            $('gotoInput').value = '';
        }

        function goToZero() {
            vib();
            apiCall('/api/position', 'POST', 0, 'position');
        }

        function setZero() {
            if(confirm('Set current position as zero?')) {
                apiCall('/api/zero', 'POST');
            }
        }

        function rebootDevice() {
            if(confirm('Reboot device? This will restart the ESP32.')) {
                apiCall('/api/reboot', 'POST');
                showToast('Device rebooting...', 'warning');
                setTimeout(() => {
                    location.reload();
                }, 5000);
            }
        }

        function saveCfg(type) {
            const endpoint = type === 'max' ? '/api/settings/max' : '/api/settings/stepsperrot';
            const paramName = type === 'max' ? 'maxSteps' : 'stepsPerRot';
            const val = type === 'max' ? $('maxInput').value : $('stepsRotInput').value;
            if(!val) return;
            apiCall(endpoint, 'POST', parseInt(val), paramName).then(() => {
                showToast('Settings saved!', 'success');
            });
        }

        // Initialize
        connectWebSocket();
        
        // Fallback polling if WebSocket fails
        setInterval(() => {
            if(!ws || ws.readyState !== WebSocket.OPEN) {
                fetch('/api/status')
                    .then(r => r.json())
                    .then(updateUI)
                    .catch(() => {});
            }
        }, 1000);
    </script>
</body>
</html>
)rawliteral";

#endif // WEB_INTERFACE_H
