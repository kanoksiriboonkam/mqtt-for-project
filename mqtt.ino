#include <WiFi.h>
#include <PubSubClient.h>
#include <ESP32Servo.h>

// WiFi Credentials
const char* ssid = "Khim";
const char* password = "123456789";

// MQTT Broker
const char* mqtt_server = "test.mosquitto.org";

// MQTT Client
WiFiClient espClient;
PubSubClient client(espClient);

// Servo setup
Servo servo;
const int servoPin = 21;

// IR Sensor pins for object detection (1-10)
const int irPins[] = {32, 14, 34, 15, 22, 18, 16, 17, 27, 13}; // IR Sensors 1-10
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
  "kmitl/project/irsensor/10"
};

// Timer for servo control
unsigned long servoStartTime = 0;
bool servoActive = false;

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

// MQTT callback function
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String receivedTopic = String(topic);
  String receivedMessage;
  
  for (unsigned int i = 0; i < length; i++) {
    receivedMessage += (char)payload[i];
  }

  Serial.print("Message received: ");
  Serial.print("Topic: ");
  Serial.print(receivedTopic);
  Serial.print(", Message: ");
  Serial.println(receivedMessage);
}

// Reconnect to MQTT Broker
void reconnect() {
  while (!client.connected()) {
    Serial.println("Connecting to MQTT...");
    if (client.connect("ESP32_Client")) {
      Serial.println("Connected to MQTT Broker");
      
      // Subscribe to all topics
      for (int i = 0; i < numSensors; i++) {
        client.subscribe(topics[i].c_str());
      }
    } else {
      Serial.print("Failed with state ");
      Serial.println(client.state());
      delay(2000);
    }
  }
}

void setup() {
  Serial.begin(115200);

  // Setup pins for IR sensors 1-10
  for (int i = 0; i < numSensors; i++) {
    pinMode(irPins[i], INPUT);
  }

  // Setup servo
  servo.attach(servoPin);
  servo.write(0); // Initial position

  // Setup WiFi and MQTT
  setupWiFi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(mqttCallback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Handle object detection with IR sensors 1-10
  for (int i = 0; i < numSensors; i++) {
    int irState = digitalRead(irPins[i]);
    String message = String(irState == LOW ? 1 : 0); // Send 1 when LOW (object detected), 0 when HIGH (no object)

    // Display the status of the sensor on the Serial Monitor
    Serial.print("IR Sensor ");
    Serial.print(i + 1);
    Serial.print(": ");
    Serial.println(irState == LOW ? "Object detected (1)" : "No object (0)");

    // Publish sensor data via MQTT
    client.publish(topics[i].c_str(), message.c_str());
  }

  // IR Sensor 9 controls the servo
  int irStateSensor9 = digitalRead(irPins[8]); // Sensor 9 is at index 8 in the array
  if (irStateSensor9 == LOW && !servoActive) {
    servo.write(90); // Raise the servo
    servoStartTime = millis(); // Start the timer
    servoActive = true; // Set servo as active
    Serial.println("Servo raised to 90 degrees.");
  }

  // Return servo to initial position after 5 seconds
  if (servoActive && millis() - servoStartTime >= 5000) {
    servo.write(0); // Lower the servo
    servoActive = false; // Reset servo state
    Serial.println("Servo returned to initial position.");
  }

  delay(100); // Ensure loop stability
}
