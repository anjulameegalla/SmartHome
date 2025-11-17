// Include all required libraries
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <WiFi.h>
#include <PubSubClient.h> // --- NEW ---

// --- Your Wi-Fi Credentials ---
const char* ssid = "Dialog 4G 696";
const char* password = "5D2345F1";

// --- NEW: MQTT Broker ---
const char* mqtt_server = "broker.hivemq.com";
WiFiClient espClient;
PubSubClient client(espClient);

// --- Sensor & Output Pins ---
#define DHTPIN 4
#define MQ_PIN 34
#define PIR_PIN 23
#define LED_PIN 25

// --- Sensor Objects ---
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// --- Global variables ---
float global_humidity = 0;
float global_temperature = 0;
int   global_gasValue = 0;
int   global_pirState = LOW;

// --- Non-Blocking Timer ---
unsigned long previousSensorRead = 0;
const long sensorInterval = 5000; // Send data every 5 seconds

// --- NEW: Function to reconnect to MQTT ---
void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP32-";
    clientId += String(random(0xffff), HEX);
    
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  
  // Set pin modes
  pinMode(MQ_PIN, INPUT);
  pinMode(PIR_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  
  dht.begin();
  
  Serial.println("PIR calibrating (10s)...");
  delay(10000); // Shorter calibration for testing
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
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // --- NEW: Point client to the MQTT server ---
  client.setServer(mqtt_server, 1883);
}

void loop() {
  // --- NEW: Check MQTT connection ---
  if (!client.connected()) {
    reconnect();
  }
  client.loop(); // This handles MQTT communication

  // --- Non-blocking sensor read ---
  unsigned long currentMillis = millis();
  if (currentMillis - previousSensorRead >= sensorInterval) {
    previousSensorRead = currentMillis;

    // --- Read sensors ---
    global_humidity = dht.readHumidity();
    global_temperature = dht.readTemperature();
    global_gasValue = analogRead(MQ_PIN);

    // Check for valid DHT readings
    if (isnan(global_humidity) || isnan(global_temperature)) {
      Serial.println("Failed to read from DHT sensor!");
      return;
    }

    // --- NEW: Publish data to MQTT ---
    
    // We use a "base topic" to keep your data organized
    // Use something unique! Maybe "anjula/project/"
    
    client.publish("anjula/project/temperature", String(global_temperature).c_str(), true);
    client.publish("anjula/project/humidity", String(global_humidity).c_str(), true);
    client.publish("anjula/project/gas", String(global_gasValue).c_str(), true);
    
    Serial.println("Sensor data published!");
  }

  // --- PIR logic (runs every loop, so it's fast) ---
  global_pirState = digitalRead(PIR_PIN);
  if (global_pirState == HIGH) {
    digitalWrite(LED_PIN, HIGH);
    // --- NEW: Publish motion event instantly ---
    client.publish("anjula/project/motion", "DETECTED", true);
  } else {
    digitalWrite(LED_PIN, LOW);
  }
}