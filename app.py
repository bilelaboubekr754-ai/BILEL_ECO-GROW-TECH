from flask import Flask, render_template, request, jsonify, session, redirect, url_for
import os

app = Flask(__name__)
app.secret_key = os.environ.get('SECRET_KEY', 'default_secret_key_12345')

# Store latest sensor data
sensor_data = {
    "temperature": 0,
    "humidity": 0,
    "water_level": 0
}

# Store device states
device_states = {
    "relay1": 0,
    "relay2": 0,
    "relay3": 0,
    "pump": 0,
    "light": 0
}

# Credentials
USERNAME = os.environ.get('ADMIN_USERNAME', 'admin')
PASSWORD = os.environ.get('ADMIN_PASSWORD', '1234')

@app.route('/login', methods=['GET', 'POST'])
def login():
    if request.method == 'POST':
        username = request.form.get('username')
        password = request.form.get('password')
        if username == USERNAME and password == PASSWORD:
            session['logged_in'] = True
            return redirect(url_for('dashboard'))
        else:
            return render_template('login.html', error='Invalid Credentials')
    return render_template('login.html')

@app.route('/logout')
def logout():
    session.pop('logged_in', None)
    return redirect(url_for('login'))

@app.route('/')
def dashboard():
    if not session.get('logged_in'):
        return redirect(url_for('login'))
    return render_template('index.html')

@app.route('/data', methods=['POST'])
def update_data():
    global sensor_data
    try:
        data = request.get_json()
        if data:
            sensor_data['temperature'] = data.get('temperature', sensor_data['temperature'])
            sensor_data['humidity'] = data.get('humidity', sensor_data['humidity'])
            sensor_data['water_level'] = data.get('water_level', sensor_data['water_level'])
            return jsonify({"status": "success", "message": "Data updated"}), 200
        return jsonify({"status": "error", "message": "Invalid JSON"}), 400
    except Exception as e:
        return jsonify({"status": "error", "message": str(e)}), 400

@app.route('/get_data', methods=['GET'])
def get_data():
    if not session.get('logged_in'):
        return jsonify({"error": "Unauthorized"}), 401
    return jsonify(sensor_data)

@app.route('/control', methods=['POST'])
def control_device():
    global device_states
    if not session.get('logged_in'):
        return jsonify({"error": "Unauthorized"}), 401
    
    try:
        data = request.get_json()
        device = data.get('device')
        state = data.get('state')
        
        if device in device_states and state in [0, 1]:
            device_states[device] = state
            return jsonify({"status": "success", "device": device, "state": state}), 200
        return jsonify({"status": "error", "message": "Invalid device or state"}), 400
    except Exception as e:
        return jsonify({"status": "error", "message": str(e)}), 400

@app.route('/commands', methods=['GET'])
def get_commands():
    # ESP8266 logic will fetch this
    return jsonify(device_states)

if __name__ == '__main__':
    port = int(os.environ.get('PORT', 5000))
    app.run(host='0.0.0.0', port=port, debug=True)
