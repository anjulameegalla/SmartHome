// Include all required libraries
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <WiFi.h>
#include <PubSubClient.h>

// --- Wi-Fi & MQTT (No change) ---
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
const char* mqtt_server = "broker.hivemq.com";
WiFiClient espClient;
PubSubClient client(espClient);

// --- NEW: Define command topic ---
#define LED_SET_TOPIC "anjula/project/led/set"

// --- Sensor Pins (No change) ---
#define DHTPIN 4
#define MQ_PIN 34
#define PIR_PIN 23
#define LED_PIN 25

// ... (Other objects and variables) ...
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);
float global_humidity = 0;
float global_temperature = 0;
int   global_gasValue = 0;
int   global_pirState = LOW;
unsigned long previousSensorRead = 0;
const long sensorInterval = 5000;


// --- --- NEW: MQTT Callback Function --- ---
// This function is called every time a message arrives on a topic we subscribe to
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  
  // Convert payload to a string
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.println(message);

  // Check if the message is for our LED
  if (String(topic) == LED_SET_TOPIC) {
    if (message == "ON") {
      Serial.println("Turning LED ON");
      digitalWrite(LED_PIN, HIGH);
    } else if (message == "OFF") {
      Serial.println("Turning LED OFF");
      digitalWrite(LED_PIN, LOW);
    }
  }
}
// --- --- END NEW --- ---


void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    String clientId = "ESP32-";
    clientId += String(random(0xffff), HEX);
    
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      // --- NEW: Subscribe to the command topic ---
      client.subscribe(LED_SET_TOPIC);
      Serial.print("Subscribed to: ");
      Serial.println(LED_SET_TOPIC);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  
  pinMode(MQ_PIN, INPUT);
  pinMode(PIR_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT); // LED is an output
  digitalWrite(LED_PIN, LOW); // Start with LED off
  
  dht.begin();
  
  Serial.println("PIR calibrating (10s)...");
  delay(10000);
  Serial.println("Calibration complete.");

  // --- Connect to Wi-Fi ---
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected.");
  Serial.println(WiFi.localIP());

  client.setServer(mqtt_server, 1883);
  // --- NEW: Tell the client to use our callback function ---
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop(); // This checks for incoming messages

  // --- Sensor reading loop (No change) ---
  unsigned long currentMillis = millis();
  if (currentMillis - previousSensorRead >= sensorInterval) {
    previousSensorRead = currentMillis;

    global_humidity = dht.readHumidity();
    global_temperature = dht.readTemperature();
    global_gasValue = analogRead(MQ_PIN);

    if (isnan(global_humidity) || isnan(global_temperature)) {
      Serial.println("Failed to read from DHT sensor!");
      return;
    }
    
    client.publish("anjula/project/temperature", String(global_temperature).c_str(), true);
    client.publish("anjula/project/humidity", String(global_humidity).c_str(), true);
    client.publish("anjula/project/gas", String(global_gasValue).c_str(), true);
    
    Serial.println("Sensor data published!");
  }

  // --- --- UPDATED PIR LOGIC --- ---
  // The LED is no longer controlled by the PIR
  global_pirState = digitalRead(PIR_PIN);
  if (global_pirState == HIGH) {
    client.publish("anjula/project/motion", "DETECTED", true);
  }
  // --- --- END UPDATED --- ---
}