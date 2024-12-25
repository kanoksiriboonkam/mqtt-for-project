#include <WiFi.h>
#include <PubSubClient.h>
#include <ESP32Servo.h>

// WiFi Credentials
const char* ssid = "Jangnubburengnong";
const char* password = "12345678";

// MQTT Broker
const char* mqtt_server = "test.mosquitto.org";

// MQTT Client
WiFiClient espClient;
PubSubClient client(espClient);

// Servo setup
Servo servo;
const int servoPin = 21;
int objectCount = 0;

// IR Sensor pins
const int irPins[] = {13, 12, 14, 27, 2, 4, 16, 17, 19};
const int numSensors = sizeof(irPins) / sizeof(irPins[0]);

// Topics
String topics[] = {
  "kmitl/project/irsensor/1",
  "kmitl/project/irsensor/2",
  "kmitl/project/irsensor/3",
  "kmitl/project/irsensor/4",
  "kmitl/project/irsensor/5",
  "kmitl/project/irsensor/6",
  "kmitl/project/irsensor/7",
  "kmitl/project/irsensor/8",
  "kmitl/project/irsensor/9",
  "kmitl/project/irsensor/count"
};

// Setup WiFi
void setupWiFi() {
  delay(10);
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
}

// Reconnect to MQTT Broker
void reconnect() {
  while (!client.connected()) {
    Serial.println("Connecting to MQTT...");
    if (client.connect("khim/ngai/za")) {
      Serial.println("connected");
    } else {
      Serial.print("failed with state ");
      Serial.println(client.state());
      delay(2000);
    }
  }
}

void setup() {
  Serial.begin(115200);

  // Setup pins for IR sensors
  for (int i = 0; i < numSensors; i++) {
    pinMode(irPins[i], INPUT);
  }

  // Setup servo
  servo.attach(servoPin);
  servo.write(0); // Initial position

  // Setup WiFi and MQTT
  setupWiFi();
  client.setServer(mqtt_server, 1883);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  for (int i = 0; i < numSensors; i++) {
    int irState = digitalRead(irPins[i]);
    String message = String(irState); // Publish 1 if LOW, 0 if HIGH

    // Handle specific sensor logic
    if (i == 7 && irState == LOW) { // IR Sensor 8 with Servo
      servo.write(90);
      delay(500);
      servo.write(0);
      objectCount++;
      client.publish(topics[9].c_str(), String(objectCount).c_str());
    }

    if (i == 8 && irState == LOW) { // IR Sensor 9
      if (objectCount > 0) {
        objectCount--;
      }
      client.publish(topics[9].c_str(), String(objectCount).c_str());
    }

    client.publish(topics[i].c_str(), message.c_str());
    delay(100);
  }
}
