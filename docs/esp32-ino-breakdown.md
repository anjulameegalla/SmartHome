# esp32_sketch.ino Breakdown

Here is a line-by-line breakdown of the ESP32 Arduino IDE code provided in esp32_sketch.ino


## 1. Libraries and Network Configuration
This section imports the necessary drivers and defines the network credentials required for connectivity.

```C++
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <WiFi.h>
#include <PubSubClient.h>

const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
const char* mqtt_server = "broker.hivemq.com";

WiFiClient espClient;
PubSubClient client(espClient);
```

- Libraries: The code uses `WiFi.h` for network connectivity and `PubSubClient.h` for MQTT communication (a lightweight messaging protocol). It also includes `DHT.h` and `Adafruit_Sensor.h` to interact with the temperature/humidity sensor.
- Clients: It initializes a WiFiClient to handle the TCP connection and passes it to the PubSubClient to handle the MQTT protocol layer.
- Broker: It is configured to connect to `broker.hivemq.com`, a public MQTT broker used for testing.


## 2. Topic Definitions and Pin Mapping
This section defines the communication "channels" (topics) and maps the physical wiring to the microcontroller's GPIO pins.

```C++
#define LED_SET_TOPIC "anjula/project/led/set"
#define RELAY_SET_TOPIC "anjula/project/relay/set"
#define DHT_SET_TOPIC "anjula/project/dht/set"
// ... other topics ...

#define DHTPIN 4
#define MQ_PIN 34
#define PIR_PIN 23
#define LED_PIN 25
#define RELAY_PIN 26

#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);
```

- MQTT Topics: The `#define` macros establish the specific paths the device will listen to. For example, sending a message to `anjula/project/relay/set` allows a remote user to trigger the relay.
- Pin Definitions: The code maps specific GPIO pins to hardware components. Note that `MQ_PIN` (Gas) is on GPIO 34, which is an input-only pin on the ESP32, suitable for analog reading. The Relay is on GPIO 26.
- Sensor Object: The `DHT dht(...)` line initializes the software interface for the sensor.


## 3. Global State Management
These variables maintain the state of the system, including sensor readings, configuration flags, and timers.

```C++
bool isDhtEnabled = true;
bool isGasEnabled = true;
bool isPirEnabled = true;

float global_humidity = 0;
// ... other vars ...

unsigned long previousSensorRead = 0;
const long sensorInterval = 5000;
```

- Control Flags: The boolean flags (`isDhtEnabled`) act as switches. They determine whether a specific sensor function should run. These can be toggled remotely via MQTT.
- Global Variables: Variables like `global_humidity` store the latest readings so they can be accessed by any function in the code.
- Non-Blocking Timer: `sensorInterval` is set to 5000ms (5 seconds). This is used with `millis()` to allow the code to multitask rather than pausing execution.


## 4. The Message Handler (Callback)
This critical function executes automatically whenever a message arrives from the MQTT broker. It acts as the "remote control" logic.

```C++
void callback(char* topic, byte* payload, unsigned int length) {
  // ... (String reconstruction) ...
  
  if (String(topic) == RELAY_SET_TOPIC) {
    if (message == "ON") {
      digitalWrite(RELAY_PIN, LOW); // Active Low: LOW = ON
    } else {
      digitalWrite(RELAY_PIN, HIGH); // Active Low: HIGH = OFF
    }
  }
  // ... (Other topic checks) ...
}
```

- Payload parsing: The function first converts the incoming raw bytes into a readable String.
- Relay Logic (Active Low): The code explicitly handles "Active Low" logic. Most relay modules trigger when the signal is grounded (LOW). Therefore, receiving "ON" results in writing LOW to the pin, and "OFF" results in writing HIGH.
- Configuration: It also listens for commands to enable/disable specific sensors by updating the boolean flags defined earlier.


## 5. Network Reconnection Logic
This function ensures the device stays connected to the server and re-establishes settings if the connection drops.

```C++
void reconnect() {
  while (!client.connected()) {
    String clientId = "ESP32-" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      client.subscribe(LED_SET_TOPIC);
      client.subscribe(RELAY_SET_TOPIC);
      // ... subscribe to others ...
    } else {
      delay(5000);
    }
  }
}
```

- Client ID: It generates a random Client ID (e.g., ESP32-A1B2) every time it connects. This is crucial for public brokers, if two devices use the same ID, the broker will disconnect the first one.
- Subscription: Upon a successful connection, the device must re-subscribe to all control topics `(.../set)`. If this step is skipped, the device will be online but will ignore all commands.


## 6. Sensor Algorithms (Gas Sensor)
This section handles the analog data processing for the MQ2 gas sensor.

```C++
float getPPM() {
  int adc = analogRead(MQ_PIN);
  float volt = adc * (3.3 / 4095.0);
  // ... calculation ...
  return 3000 * pow(ratio, -2.05);
}
```

- Calibration: The `calibrateMQ2` function (called in setup) measures the sensor's resistance in clean air to establish a baseline `($R_0$)`.
- PPM Calculation: The `getPPM` function reads the analog voltage (0-3.3V), calculates the sensor's current resistance `($R_S$)`, and compares it to the baseline `($R_S/R_0$)`. It then uses a power law formula `($y = a \cdot x^b$)` to estimate the gas concentration in PPM.


## 7. Main Execution Loop
The `loop()` function manages the system's multitasking capabilities.

```C++
void loop() {
  if (!client.connected()) reconnect();
  client.loop();

  unsigned long currentMillis = millis();
  if (currentMillis - previousSensorRead >= sensorInterval) {
    previousSensorRead = currentMillis;
    // ... Read DHT and Gas and Publish ...
  }

  if (isPirEnabled) {
    int currentPirState = digitalRead(PIR_PIN);
    if (currentPirState == HIGH && lastPirState == LOW) {
      client.publish("anjula/project/motion", "DETECTED", true);
    }
    lastPirState = currentPirState;
  }
}
```

- Maintenance: `client.loop()` is called essentially to keep the MQTT connection alive and process incoming messages.
- Periodic Tasks (Non-blocking): The code checks if `5000ms` have passed since the last read. If so, it reads the DHT and Gas sensors and publishes the data. This prevents the use of `delay()`, which would freeze the system.
- Real-time Tasks (PIR): The Motion detection logic is placed outside the timer check. It runs in every cycle of the loop.
- Edge Detection: It checks if the sensor went from LOW to HIGH. This ensures a message is sent only at the moment motion is detected, rather than continuously sending messages while motion is present.


<br>