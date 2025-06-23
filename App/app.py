# app.py
from flask import Flask, render_template, request, jsonify, redirect, url_for, session
from flask_sock import Sock
from flask_cors import CORS
from config import PASSKEY
import json, time, sqlite3, os
from config import SENSORS, ACTUATORS, FLAGS, DB_FILE, STATE_FILE

app = Flask(__name__)
sock = Sock(app)
CORS(app)

# --- State Tracking ---
state = {act: False for act in ACTUATORS}
flag_data = {flag: None for flag in FLAGS}
sensor_data = {sen: None for sen in SENSORS}

# WebSocket clients
ws_clients = []

# Heartbeat tracking
last_heartbeat = 0  # epoch timestamp of last heartbeat

app.secret_key = 'Voldemort@5400'  # needed for sessions

# --- Ensure SQLite DB setup ---
def init_db():
    conn = sqlite3.connect(DB_FILE)
    c = conn.cursor()
    c.execute('''CREATE TABLE IF NOT EXISTS sensor_data (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    timestamp REAL,
                    sensor TEXT,
                    value REAL
                )''')
    conn.commit()
    conn.close()

init_db()

# --- Load persisted actuator/flag state ---
if os.path.exists(STATE_FILE):
    with open(STATE_FILE, 'r') as f:
        saved = json.load(f)
        state.update(saved.get('actuators', {}))
        flag_data.update(saved.get('flags', {}))

# --- Flask Routes ---
@app.route('/')
def dashboard():
    if session.get("authenticated") != True:
        return redirect(url_for('login'))
    return render_template('dashboard.html', sensors=sensor_data, actuators=state, flags=flag_data)

@app.route('/login', methods=['GET', 'POST'])
def login():
    error = None
    if request.method == 'POST':
        entered = request.form.get("passkey", "").lower()
        if entered == PASSKEY:
            session['authenticated'] = True
            return redirect(url_for('dashboard'))
        else:
            error = "Incorrect passkey"
    return render_template('login.html', error=error)


@app.route('/api/sensors', methods=['POST'])
def update_sensors():
    global sensor_data
    data = request.get_json()
    changed = False

    conn = sqlite3.connect(DB_FILE)
    c = conn.cursor()

    now = time.time()
    for s in SENSORS:
        if s in data:
            sensor_data[s] = data[s]
            # Store every received value
            c.execute("INSERT INTO sensor_data (timestamp, sensor, value) VALUES (?, ?, ?)", (now, s, data[s]))
            changed = True

    conn.commit()
    conn.close()

    if changed:
        send_sensor_update()

    return jsonify({'status': 'sensor data logged'}), 200

@app.route('/api/sensors', methods=['GET'])
def get_sensors():
    return jsonify(sensor_data)

@app.route('/api/sensor-history')
def sensor_history():
    sensor = request.args.get('sensor')
    limit = int(request.args.get('limit', 1000))

    if sensor not in SENSORS:
        return jsonify({'error': 'Invalid sensor'}), 400

    conn = sqlite3.connect(DB_FILE)
    c = conn.cursor()
    c.execute("SELECT timestamp, value FROM sensor_data WHERE sensor=? ORDER BY timestamp DESC LIMIT ?", (sensor, limit))
    rows = c.fetchall()
    conn.close()

    history = [{'timestamp': ts, 'value': val} for ts, val in reversed(rows)]
    return jsonify(history)

@app.route('/api/actuators/<name>', methods=['POST'])
def control_actuator(name):
    if name not in ACTUATORS:
        return jsonify({'error': 'Invalid actuator'}), 400

    req = request.get_json()
    new_state = req.get('state', False)

    if state[name] != new_state:
        state[name] = new_state
        save_state()
        send_actuator_update()

    return jsonify({'status': f'{name} set to {state[name]}'}), 200

@app.route('/api/actuators', methods=['GET'])
def get_actuators():
    return jsonify(state)

@app.route('/api/flags', methods=['GET'])
def get_flags():
    return jsonify(flag_data)

@app.route('/api/flags', methods=['POST'])
def update_flags():
    global flag_data
    data = request.get_json()
    changed = False

    for flag in FLAGS:
        if flag in data and flag_data.get(flag) != data[flag]:
            flag_data[flag] = data[flag]
            changed = True

    if changed:
        save_state()
        send_flag_update()

    return jsonify({'status': 'flags updated'}), 200

@app.route('/api/time')
def get_time():
    return jsonify({"epoch": int(time.time())})

# --- Heartbeat routes ---
@app.route('/heartbeat', methods=['POST'])
def heartbeat():
    global last_heartbeat
    last_heartbeat = time.time()
    # Broadcast to clients if needed
    msg = json.dumps({'type': 'heartbeat', 'data': 'online'})
    broadcast(msg)
    return jsonify({'status': 'heartbeat received'}), 200

@app.route('/api/heartbeat')
def get_heartbeat():
    now = time.time()
    online = (now - last_heartbeat) < 10  # Considered online if < 10s
    return jsonify({
        'status': 'online' if online else 'offline',
        'last_heartbeat': int(last_heartbeat)
    })

# --- WebSocket Handler ---
@sock.route('/ws')
def websocket(ws):
    ws_clients.append(ws)

    try:
        # Send current state
        ws.send(json.dumps({'type': 'sensor_update', 'data': sensor_data}))
        ws.send(json.dumps({'type': 'actuator_update', 'data': state}))
        ws.send(json.dumps({'type': 'flag_update', 'data': flag_data}))

        while True:
            msg = ws.receive()
            if msg:
                data = json.loads(msg)
                msg_type = data.get("type")
                content = data.get("data")
                source = data.get("source", "unknown")

                if msg_type == "actuator_status" and source == "device":
                    changed = False
                    for key, value in content.items():
                        if key in state and state[key] != value:
                            state[key] = value
                            changed = True
                    if changed:
                        save_state()
                        send_actuator_update(source_ws=ws)

                elif msg_type == "flag_status" and source == "device":
                    changed = False
                    for key, value in content.items():
                        if key in flag_data and flag_data[key] != value:
                            flag_data[key] = value
                            changed = True
                    if changed:
                        save_state()
                        send_flag_update(source_ws=ws)

    except Exception as e:
        print("WebSocket error:", e)
    finally:
        if ws in ws_clients:
            ws_clients.remove(ws)

# --- Broadcast + Save State ---
def save_state():
    with open(STATE_FILE, 'w') as f:
        json.dump({'actuators': state, 'flags': flag_data}, f)

def send_sensor_update():
    msg = json.dumps({'type': 'sensor_update', 'data': sensor_data})
    broadcast(msg)

def send_actuator_update(source_ws=None):
    msg = json.dumps({'type': 'actuator_update', 'data': state, 'source': 'server'})
    for client in ws_clients[:]:
        if client != source_ws:
            try:
                client.send(msg)
            except:
                ws_clients.remove(client)

def send_flag_update(source_ws=None):
    msg = json.dumps({'type': 'flag_update', 'data': flag_data, 'source': 'server'})
    for client in ws_clients[:]:
        if client != source_ws:
            try:
                client.send(msg)
            except:
                ws_clients.remove(client)

def broadcast(msg):
    for client in ws_clients[:]:
        try:
            client.send(msg)
        except:
            ws_clients.remove(client)

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)
