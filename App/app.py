from flask import Flask, render_template, request, jsonify
from flask_sock import Sock
import json, time
from config import SENSORS, ACTUATORS, DATA_FILE

app = Flask(__name__)
sock = Sock(app)

# State variables
state = {act: False for act in ACTUATORS}
sensor_data = {sen: None for sen in SENSORS}
ws_clients = []

@app.route('/')
def dashboard():
    return render_template('dashboard.html', sensors=sensor_data, actuators=state)

@app.route('/api/sensors', methods=['POST'])
def update_sensors():
    global sensor_data
    data = request.get_json()
    for s in SENSORS:
        if s in data:
            sensor_data[s] = data[s]

    # Log data
    with open(DATA_FILE, 'w') as f:
        json.dump({'timestamp': time.time(), 'data': sensor_data}, f)

    # Emit to dashboard clients only (NodeMCU can ignore)
    socketio.emit('sensor_update', {'type': 'sensor_update', 'data': sensor_data})
    return jsonify({'status': 'updated'}), 200

@app.route('/api/actuators/<name>', methods=['POST'])
def control_actuator(name):
    if name not in ACTUATORS:
        return jsonify({'error': 'Invalid actuator'}), 400
    req = request.get_json()
    state[name] = req.get('state', False)

    # Broadcast actuator update to all WebSocket clients
    send_actuator_update()
    return jsonify({'status': f'{name} set to {state[name]}'}), 200

@app.route('/api/actuators', methods=['GET'])
def get_actuators():
    return jsonify(state)
    @app.route('/api/sensors', methods=['GET'])
def get_sensors():
    return jsonify(sensor_data)

# WebSocket route for communication with NodeMCU clients
@sock.route('/ws')
def websocket(ws):
    # Add the new client to the list
    ws_clients.append(ws)

    # Send initial data to the newly connected client
    ws.send(json.dumps({'type': 'sensor_update', 'data': sensor_data}))
    ws.send(json.dumps({'type': 'actuator_update', 'data': state}))

    try:
        while True:
            message = ws.receive()
            if message:
                print("Received message:", message)
                # Handle received WebSocket messages (if needed)
    except:
        # Remove the client when disconnected
        ws_clients.remove(ws)
        # Function to send sensor update to all WebSocket clients
def send_sensor_update():
    message = json.dumps({'type': 'sensor_update', 'data': sensor_data})
    for client in ws_clients:
        try:
            client.send(message)
        except:
            pass  # Ignore failed clients

# Function to send actuator update to all WebSocket clients
def send_actuator_update():
    message = json.dumps({'type': 'actuator_update', 'data': state})
    for client in ws_clients:
        try:
            client.send(message)
        except:
            pass  # Ignore failed clients

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)