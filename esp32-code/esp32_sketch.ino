// Include all required libraries
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <WiFi.h>
#include <PubSubClient.h>

// --- Wi-Fi ---
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// --- MQTT public broker ---
const char* mqtt_server = "broker.hivemq.com";
WiFiClient espClient;
PubSubClient client(espClient);

// --- All command topics ---
#define LED_SET_TOPIC "anjula/project/led/set"
#define DHT_SET_TOPIC "anjula/project/dht/set"
#define GAS_SET_TOPIC "anjula/project/gas/set"
#define PIR_SET_TOPIC "anjula/project/pir/set"

// --- Sensor Pins ---
#define DHTPIN 4
#define MQ_PIN 34
#define PIR_PIN 23
#define LED_PIN 25

#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// --- Sensor Enable/Disable Flags ---
bool isDhtEnabled = true;
bool isGasEnabled = true;
bool isPirEnabled = true;

// --- Global variables ---
float global_humidity = 0;
float global_temperature = 0;
int   global_gasValue = 0;
int   global_pirState = LOW;
unsigned long previousSensorRead = 0;
const long sensorInterval = 5000;


// --- MQTT Callback Function ---
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.println(message);

  // Check which topic the command came from
  if (String(topic) == LED_SET_TOPIC) {
    if (message == "ON") {
      Serial.println("Turning LED ON");
      digitalWrite(LED_PIN, HIGH);
    } else if (message == "OFF") {
      Serial.println("Turning LED OFF");
      digitalWrite(LED_PIN, LOW);
    }
  } 
  else if (String(topic) == DHT_SET_TOPIC) {
    isDhtEnabled = (message == "ON");
    Serial.print("DHT Sensor "); Serial.println(isDhtEnabled ? "Enabled" : "Disabled");
  } 
  else if (String(topic) == GAS_SET_TOPIC) {
    isGasEnabled = (message == "ON");
    Serial.print("Gas Sensor "); Serial.println(isGasEnabled ? "Enabled" : "Disabled");
  } 
  else if (String(topic) == PIR_SET_TOPIC) {
    isPirEnabled = (message == "ON");
    Serial.print("PIR Sensor "); Serial.println(isPirEnabled ? "Enabled" : "Disabled");
  }
}


// --- Reconnect Function ---
void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    String clientId = "ESP32-";
    clientId += String(random(0xffff), HEX);
    
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      
      // Subscribe to all command topics
      client.subscribe(LED_SET_TOPIC);
      client.subscribe(DHT_SET_TOPIC);
      client.subscribe(GAS_SET_TOPIC);
      client.subscribe(PIR_SET_TOPIC);
      
      Serial.println("Subscribed to all command topics.");
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
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  
  dht.begin();
  
  Serial.println("PIR calibrating (10s)...");
  delay(10000);
  Serial.println("Calibration complete.");

  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected.");
  Serial.println(WiFi.localIP());

  client.setServer(mqtt_server, 1883); // Back to port 1883
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop(); // This checks for incoming messages

  // --- Sensor reading loop ---
  unsigned long currentMillis = millis();
  if (currentMillis - previousSensorRead >= sensorInterval) {
    previousSensorRead = currentMillis;

    // --- Only read/publish if enabled ---
    if (isDhtEnabled) {
      global_humidity = dht.readHumidity();
      global_temperature = dht.readTemperature();

      if (isnan(global_humidity) || isnan(global_temperature)) {
        Serial.println("Failed to read from DHT sensor!");
      } else {
        client.publish("anjula/project/temperature", String(global_temperature).c_str(), true);
        client.publish("anjula/project/humidity", String(global_humidity).c_str(), true);
        Serial.println("DHT data published!");
      }
    }
    
    // --- Only read/publish if enabled ---
    if (isGasEnabled) {
      global_gasValue = analogRead(MQ_PIN);
      client.publish("anjula/project/gas", String(global_gasValue).c_str(), true);
      Serial.println("Gas data published!");
    }
  }

  // --- PIR LOGIC ---
  if (isPirEnabled) {
    global_pirState = digitalRead(PIR_PIN);
    if (global_pirState == HIGH) {
      client.publish("anjula/project/motion", "DETECTED", true);
    }
  }
}