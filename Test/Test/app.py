from flask import Flask, render_template, request, jsonify
from flask_sock import Sock
from flask_cors import CORS
import json, time
from config import SENSORS, ACTUATORS, FLAGS, DATA_FILE

app = Flask(__name__)
sock = Sock(app)
CORS(app)

# Track states
state = {act: False for act in ACTUATORS}
sensor_data = {sen: None for sen in SENSORS}
flag_data = {flag: None for flag in FLAGS}

ws_clients = []

@app.route('/')
def dashboard():
    return render_template(
        'dashboard.html',
        sensors=sensor_data,
        actuators=state,
        flags=flag_data
    )

@app.route('/api/sensors', methods=['POST'])
def update_sensors():
    global sensor_data
    data = request.get_json()
    changed = False

    for s in SENSORS:
        if s in data and sensor_data.get(s) != data[s]:
            sensor_data[s] = data[s]
            changed = True

    if changed:
        with open(DATA_FILE, 'w') as f:
            json.dump({'timestamp': time.time(), 'data': sensor_data}, f)
        send_sensor_update()

    return jsonify({'status': 'updated'}), 200

@app.route('/api/actuators/<name>', methods=['POST'])
def control_actuator(name):
    if name not in ACTUATORS:
        return jsonify({'error': 'Invalid actuator'}), 400

    req = request.get_json()
    new_state = req.get('state', False)

    if state[name] != new_state:
        state[name] = new_state
        send_actuator_update()

    return jsonify({'status': f'{name} set to {state[name]}'}), 200

@app.route('/api/actuators', methods=['GET'])
def get_actuators():
    return jsonify(state)

@app.route('/api/sensors', methods=['GET'])
def get_sensors():
    return jsonify(sensor_data)

@app.route('/api/time')
def get_time():
    return jsonify({"epoch": int(time.time())})

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
        send_flag_update()

    return jsonify({'status': 'flags updated'}), 200

# --- WebSocket Route ---
@sock.route('/ws')
def websocket(ws):
    ws_clients.append(ws)

    try:
        # Initial push
        ws.send(json.dumps({'type': 'sensor_update', 'data': sensor_data}))
        ws.send(json.dumps({'type': 'actuator_update', 'data': state}))
        ws.send(json.dumps({'type': 'flag_update', 'data': flag_data}))

        while True:
            message = ws.receive()
            if message:
                print("Received WebSocket message:", message)
                try:
                    data = json.loads(message)
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
                            print("Updated actuator state from device:", state)
                            send_actuator_update(source_ws=ws)

                    elif msg_type == "flag_status" and source == "device":
                        changed = False
                        for key, value in content.items():
                            if key in flag_data and flag_data[key] != value:
                                flag_data[key] = value
                                changed = True
                        if changed:
                            print("Updated flag data from device:", flag_data)
                            send_flag_update(source_ws=ws)

                except Exception as e:
                    print("Error handling WebSocket message:", e)

    except Exception as e:
        print("WebSocket disconnected:", e)
    finally:
        if ws in ws_clients:
            ws_clients.remove(ws)

# --- Broadcast functions ---
def send_sensor_update():
    message = json.dumps({'type': 'sensor_update', 'data': sensor_data})
    broadcast(message)

def send_actuator_update(source_ws=None):
    message = json.dumps({'type': 'actuator_update', 'data': state, 'source': 'server'})
    for client in ws_clients[:]:
        if client != source_ws:
            try:
                client.send(message)
            except:
                ws_clients.remove(client)

def send_flag_update(source_ws=None):
    message = json.dumps({'type': 'flag_update', 'data': flag_data, 'source': 'server'})
    for client in ws_clients[:]:
        if client != source_ws:
            try:
                client.send(message)
            except:
                ws_clients.remove(client)

def broadcast(message):
    for client in ws_clients[:]:
        try:
            client.send(message)
        except:
            ws_clients.remove(client)

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)
