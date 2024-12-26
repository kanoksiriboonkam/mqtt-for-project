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

// IR Sensor pins for object detection (1-7) and special control (8-9)
const int irPins[] = {13, 12, 14, 27, 2, 4, 16}; // IR Sensors 1-7
const int irControlPins[] = {17, 19}; // IR Sensors 8 and 9 for controlling object count and servo
const int numSensors = sizeof(irPins) / sizeof(irPins[0]);
const int numControlSensors = sizeof(irControlPins) / sizeof(irControlPins[0]);

// Topics
String topics[] = {
  "kmitl/project/irsensor/1",
  "kmitl/project/irsensor/2",
  "kmitl/project/irsensor/3",
  "kmitl/project/irsensor/4",
  "kmitl/project/irsensor/5",
  "kmitl/project/irsensor/6",
  "kmitl/project/irsensor/7",
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

  // Update objectCount if the count topic is received
  if (receivedTopic == "kmitl/project/irsensor/count") {
    objectCount = receivedMessage.toInt(); // Update objectCount
    Serial.print("Updated objectCount: ");
    Serial.println(objectCount);
  }
}

// Reconnect to MQTT Broker
void reconnect() {
  while (!client.connected()) {
    Serial.println("Connecting to MQTT...");
    if (client.connect("khim/ngai/za")) {
      Serial.println("Connected to MQTT Broker");
      client.subscribe("kmitl/project/irsensor/count"); // Subscribe to count topic
    } else {
      Serial.print("Failed with state ");
      Serial.println(client.state());
      delay(2000);
    }
  }
}

unsigned long servoStartTime = 0;
bool servoActive = false;

void setup() {
  Serial.begin(115200);

  // Setup pins for IR sensors 1-7
  for (int i = 0; i < numSensors; i++) {
    pinMode(irPins[i], INPUT);
  }

  // Setup pins for IR sensors 8 and 9 (Control Sensors)
  for (int i = 0; i < numControlSensors; i++) {
    pinMode(irControlPins[i], INPUT);
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

  // Handle object detection with IR sensors 1-7
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
    delay(100);
  }

  // Handle control sensors (8 and 9) for servo and object count logic
  for (int i = 0; i < numControlSensors; i++) {
    int irControlState = digitalRead(irControlPins[i]);

    if (i == 0 && irControlState == LOW && !servoActive) { // IR Sensor 8 with Servo
      servo.write(90); // Raise the servo
      servoStartTime = millis(); // Record the start time
      servoActive = true; // Indicate that the servo is active
      objectCount++;
      client.publish(topics[8].c_str(), String(objectCount).c_str()); // Publish the object count to MQTT

      // Display the object count
      Serial.print("Object Count (increment): ");
      Serial.println(objectCount);

      delay(500); // Adding a small delay to avoid repetitive triggering
    }

    if (i == 1 && irControlState == LOW) { // IR Sensor 9 for decrementing object count
      if (objectCount > 0) {
        objectCount--;
      }
      client.publish(topics[8].c_str(), String(objectCount).c_str()); // Publish the object count to MQTT

      // Display the object count
      Serial.print("Object Count (decrement): ");
      Serial.println(objectCount);

      delay(500); // Adding a small delay to avoid repetitive triggering
    }
  }

  // Check the time and return the servo to its original position after 5 seconds
  if (servoActive && (millis() - servoStartTime >= 5000)) {
    servo.write(0); // Return the servo to the original position
    servoActive = false; // Deactivate the servo
    Serial.println("Servo returned to original position.");
  }
}
