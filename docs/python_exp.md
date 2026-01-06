# app.py Explanation

Here is a line-by-line explanation of the Python code provided in app.py.


## 1. Imports

```Python
from flask import Flask, jsonify, render_template, request
import paho.mqtt.client as mqtt
import threading
import sqlite3
from datetime import datetime
import time
```

- flask: Imports the web framework modules to create the app (Flask), return JSON data (jsonify), render HTML pages (render_template), and handle incoming data (request).

- paho.mqtt.client: Imports the client library used to communicate with the MQTT broker (HiveMQ).

- threading: Used to run the MQTT listener in a background thread so it doesn't block the website.

- sqlite3: Imports the database library to store sensor history.

- datetime: Used to generate timestamps for the database entries.

- time: Used for delays (e.g., waiting 5 seconds before retrying a failed connection).


## 2. Flask App & Database Setup

```Python
app = Flask(__name__)
DB_NAME = "sensor_data.db"
```

Initializes the Flask application and defines the name of the SQLite database file (sensor_data.db).

```Python
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
```

- init_db function: Connects to the database and creates a table named readings if it doesn't already exist.

The table stores an auto-incrementing id, the timestamp of the reading, the topic (sensor name), and the sensor value.

## 3. MQTT Configuration

```Python
MQTT_BROKER = "broker.hivemq.com"
MQTT_PORT = 1883
TOPICS = [
    "anjula/project/temperature",
    "anjula/project/humidity",
    "anjula/project/gas",
    "anjula/project/motion"
]
```

Sets the MQTT broker address (public HiveMQ broker) and port (1883).

- TOPICS: A list of topics the app will listen to for incoming sensor data.

```Python

LED_COMMAND_TOPIC = "anjula/project/led/set"
RELAY_COMMAND_TOPIC = "anjula/project/relay/set"
DHT_COMMAND_TOPIC = "anjula/project/dht/set"
GAS_COMMAND_TOPIC = "anjula/project/gas/set"
PIR_COMMAND_TOPIC = "anjula/project/pir/set"
```

Defines the topics used to send commands back to the hardware (e.g., turning an LED or Relay ON/OFF).

## 4. Global Variables & Helpers

```Python
sensor_data = {
    "temperature": "Waiting...",
    "humidity": "Waiting...",
    "gas": "Waiting...",
    "motion": "Waiting..."
}
```

A dictionary acting as a temporary memory (cache) to hold the latest reading for each sensor.

```Python
SENSOR_MAP = {
    'temperature': {'topic': 'anjula/project/temperature', 'unit': '°C'},
    'humidity': {'topic': 'anjula/project/humidity', 'unit': '%'},
    'gas': {'topic': 'anjula/project/gas', 'unit': 'PPM'}
}
```

Maps friendly names (like temperature) to their specific MQTT topic and unit. This is likely used when drawing charts.

```Python
client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION1)
```

Creates the MQTT client instance.

```Python
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
```

- log_to_db: A helper function that takes a topic and value, connects to the database, and saves the reading with the current time.

## 5. MQTT Callbacks

```Python
def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("Connected to MQTT Broker!")
        for topic in TOPICS:
            client.subscribe(topic)
    else:
        print(f"Failed to connect, return code {rc}\n")
```

- on_connect: Triggered when the app connects to the MQTT broker. If successful (rc == 0), it subscribes to all sensor topics.

```Python
def on_message(client, userdata, msg):
    topic = msg.topic
    payload = msg.payload.decode()
    print(f"Received: {topic} = {payload}")
    
    if topic == "anjula/project/temperature": sensor_data["temperature"] = payload
    elif topic == "anjula/project/humidity": sensor_data["humidity"] = payload
    elif topic == "anjula/project/gas": sensor_data["gas"] = payload
    elif topic == "anjula/project/motion": sensor_data["motion"] = payload
    
    log_to_db(topic, payload)
```

- on_message: Triggered when a message arrives on a subscribed topic. It updates the sensor_data dictionary (for real-time display) and calls log_to_db (for history).

```Python
def mqtt_listener():
    client.on_connect = on_connect
    client.on_message = on_message
    while True:
        try:
            client.connect(MQTT_BROKER, MQTT_PORT, 60)
            client.loop_forever()
        except Exception as e:
            print(f"MQTT connection failed: {e}. Retrying in 5s...")
            time.sleep(5)
```

- mqtt_listener: This function runs the MQTT loop. It attempts to connect and keep the connection alive (loop_forever). If it crashes, it waits 5 seconds and retries.

## 6. Flask Routes (Web Pages)

```Python
@app.route('/')
def index():
    return render_template('index.html')
```
Loads the main dashboard page (index.html) when you visit the root URL (/).

```Python
@app.route('/history')
def history():
    return render_template('history.html')
```
Loads the history page (history.html).

```Python
@app.route('/data')
def get_data():
    return jsonify(sensor_data)
```
Returns the latest sensor readings as JSON. The web page likely calls this repeatedly (polling) to update the numbers without refreshing.

## 7. History API

```Python
@app.route('/api/history/<string:sensor_name>')
def get_history(sensor_name):
    config = SENSOR_MAP.get(sensor_name)
    if not config: return jsonify({"error": "Invalid sensor"}), 404
    
    conn = sqlite3.connect(DB_NAME, check_same_thread=False)
    conn.row_factory = sqlite3.Row
    c = conn.cursor()
    c.execute("SELECT timestamp, value FROM readings WHERE topic = ? ORDER BY timestamp DESC LIMIT 100", (config['topic'],))
    rows = c.fetchall()
    rows.reverse()
    conn.close()
    
    labels = [row['timestamp'] for row in rows]
    data = [float(row['value']) for row in rows]
    return jsonify({'labels': labels, 'data': data, 'y_label': config['unit']})
```

Fetches the last 100 database entries for a specific sensor.

It reverses the list so the data goes from oldest -> newest (correct for graphing).

Returns the timestamps (labels) and values (data) as JSON for a chart library (like Chart.js).

### 8. Control APIs

```Python
@app.route('/control/led', methods=['POST'])
def control_led():
    data = request.json
    state = data.get('state')
    client.publish(LED_COMMAND_TOPIC, state)
    return jsonify({"success": True})
```
Receives a JSON command (e.g., {"state": "ON"}) and publishes it to the LED topic.

```Python
@app.route('/control/relay', methods=['POST'])
def control_relay():
    data = request.json
    state = data.get('state')
    client.publish(RELAY_COMMAND_TOPIC, state)
    print(f"Relay Command: {state}")
    return jsonify({"success": True})
```
Same as above, but publishes to the Relay topic.

```Python
@app.route('/control/sensor', methods=['POST'])
def control_sensor():
    data = request.json
    sensor = data.get('sensor')
    state = data.get('state')
    
    topic = None
    if sensor == "dht": topic = DHT_COMMAND_TOPIC
    elif sensor == "gas": topic = GAS_COMMAND_TOPIC
    elif sensor == "pir": topic = PIR_COMMAND_TOPIC
    
    if topic:
        client.publish(topic, state)
        return jsonify({"success": True})
    return jsonify({"error": "Invalid sensor"}), 400
```
Allows toggling specific sensors (DHT, Gas, PIR) on/off remotely by publishing to their specific command topics.

## 9. Main Execution

```Python
if __name__ == '__main__':
    init_db()
    mqtt_thread = threading.Thread(target=mqtt_listener, daemon=True)
    mqtt_thread.start()
    app.run(host="0.0.0.0", port=5000)
```

- init_db(): Initializes the database when the script starts.

- threading.Thread(...): Starts the mqtt_listener in a separate "daemon" thread. This allows the MQTT loop to run in the background while the website runs in the foreground.

- app.run(...): Starts the Flask web server on port 5000, accessible from any IP on the network (0.0.0.0).


<br>