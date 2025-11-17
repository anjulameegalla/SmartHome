from flask import Flask, jsonify, render_template
import paho.mqtt.client as mqtt
import threading
import sqlite3
from datetime import datetime

# --- Flask App ---
app = Flask(__name__)

# --- Database ---
DB_NAME = "sensor_data.db"
def init_db():
    conn = sqlite3.connect(DB_NAME, check_same_thread=False)
    c = conn.cursor()
    c.execute('''
        CREATE TABLE IF NOT EXISTS readings (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp DATETIME,
            topic TEXT,
            value TEXT
        )
    ''')
    conn.commit()
    conn.close()
    print(f"Database '{DB_NAME}' initialized.")

# --- MQTT Client (No changes here, all your existing MQTT code) ---
MQTT_BROKER = "broker.hivemq.com"
MQTT_PORT = 1883
TOPICS = [
    "anjula/project/temperature",
    "anjula/project/humidity",
    "anjula/project/gas",
    "anjula/project/motion"
]
sensor_data = {
    "temperature": "Waiting...",
    "humidity": "Waiting...",
    "gas": "Waiting...",
    "motion": "Waiting..."
}
def log_to_db(topic, payload):
    try:
        conn = sqlite3.connect(DB_NAME, check_same_thread=False)
        c = conn.cursor()
        timestamp = datetime.now()
        c.execute("INSERT INTO readings (timestamp, topic, value) VALUES (?, ?, ?)",
                  (timestamp, topic, payload))
        conn.commit()
        conn.close()
    except sqlite3.Error as e:
        print(f"Failed to log to database: {e}")
def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("Connected to MQTT Broker!")
        for topic in TOPICS:
            client.subscribe(topic)
            print(f"Subscribed to: {topic}")
    else:
        print(f"Failed to connect, return code {rc}\n")
def on_message(client, userdata, msg):
    topic = msg.topic
    payload = msg.payload.decode()
    print(f"Received message: Topic='{topic}'  Data='{payload}'")
    log_to_db(topic, payload)
    if topic == "anjula/project/temperature": sensor_data["temperature"] = payload
    elif topic == "anjula/project/humidity": sensor_data["humidity"] = payload
    elif topic == "anjula/project/gas": sensor_data["gas"] = payload
    elif topic == "anjula/project/motion": sensor_data["motion"] = payload
def mqtt_listener():
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION1)
    client.on_connect = on_connect
    client.on_message = on_message
    client.connect(MQTT_BROKER, MQTT_PORT, 60)
    client.loop_forever()
# --- End of MQTT Code ---


# --- --- NEW: SENSOR MAPPING --- ---
# This dictionary maps the URL name to the database topic and the Y-axis unit
SENSOR_MAP = {
    'temperature': {'topic': 'anjula/project/temperature', 'unit': '°C'},
    'humidity': {'topic': 'anjula/project/humidity', 'unit': '%'},
    'gas': {'topic': 'anjula/project/gas', 'unit': 'Raw Value'}
}
# --- --- END NEW --- ---


# --- Flask Web Routes ---
@app.route('/')
def index():
    return render_template('index.html', **sensor_data)

@app.route('/data')
def get_data():
    return jsonify(sensor_data)

@app.route('/history')
def history():
    # This route just serves the HTML page, no changes
    return render_template('history.html')

# --- --- UPDATED API --- ---
# This one "smart" endpoint replaces the old one
@app.route('/api/history/<string:sensor_name>')
def get_history(sensor_name):
    # Look up the sensor config from our map
    config = SENSOR_MAP.get(sensor_name)
    
    # If sensor name is invalid, return an error
    if not config:
        return jsonify({"error": "Invalid sensor name"}), 404
        
    topic = config['topic']
    unit = config['unit']
    
    conn = sqlite3.connect(DB_NAME, check_same_thread=False)
    conn.row_factory = sqlite3.Row
    c = conn.cursor()
    
    c.execute("SELECT timestamp, value FROM readings WHERE topic = ? ORDER BY timestamp DESC LIMIT 100", (topic,))
    rows = c.fetchall()
    rows.reverse() # Put in chronological order
    conn.close()
    
    # Format for Chart.js
    labels = [row['timestamp'] for row in rows]
    data = [float(row['value']) for row in rows]
    
    # Return the Y-axis label along with the data
    return jsonify({'labels': labels, 'data': data, 'y_label': unit})
# --- --- END UPDATED API --- ---


# --- Main Program ---
if __name__ == '__main__':
    init_db() 
    mqtt_thread = threading.Thread(target=mqtt_listener, daemon=True)
    mqtt_thread.start()
    print("Starting Flask web server...")
    app.run(host="0.0.0.0", port=5000)