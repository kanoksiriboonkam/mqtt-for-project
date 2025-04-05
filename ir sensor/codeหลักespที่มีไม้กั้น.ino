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
  "kmitl/project/entrance",   // Changed from irsensor/9
  "kmitl/project/exit"        // Changed from irsensor/10
};

// Servo control variables
unsigned long servoStartTime = 0;
bool servoActive = false;

// WiFi Setup
void setupWiFi() {
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
  Serial.print(receivedTopic);
  Serial.print(" -> ");
  Serial.println(receivedMessage);
}

// Reconnect to MQTT Broker
void reconnect() {
  while (!client.connected()) {
    Serial.println("Connecting to MQTT...");
    if (client.connect("ESP32_Client")) {
      Serial.println("Connected to MQTT Broker");
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

// Task for MQTT communication
void mqttTask(void* parameter) {
  while (1) {
    if (!client.connected()) {
      reconnect();
    }
    client.loop();
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

// Task for reading IR sensors
void irSensorTask(void* parameter) {
  while (1) {
    for (int i = 0; i < numSensors; i++) {
      int irState = digitalRead(irPins[i]);
      String message = String(irState == LOW ? 1 : 0);

      if (i == 8) {
        Serial.printf("Entrance Sensor: %s\n", (irState == LOW ? "Object detected (1)" : "No object (0)"));
      } else if (i == 9) {
        Serial.printf("Exit Sensor: %s\n", (irState == LOW ? "Object detected (1)" : "No object (0)"));
      } else {
        Serial.printf("IR Sensor %d: %s\n", i + 1, (irState == LOW ? "Object detected (1)" : "No object (0)"));
      }

      client.publish(topics[i].c_str(), message.c_str());
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

// Task for controlling servo motor
void servoTask(void* parameter) {
  while (1) {
    int entranceSensorState = digitalRead(irPins[8]); // Sensor 9 (entrance)
    Serial.printf("Entrance Sensor State: %d\n", entranceSensorState);
    
    if (entranceSensorState == LOW) { // Object detected
      if (!servoActive) {
        servo.write(90); // Move servo to 90 degrees
        servoActive = true;
        servoStartTime = millis();
        Serial.println("Servo raised to 90 degrees and holding.");
      }
    } else { // No object detected
      if (servoActive && millis() - servoStartTime >= 5000) {
        servo.write(0); // Move servo back to 0 degrees
        servoActive = false;
        Serial.println("Servo returned to initial position (0 degrees). ");
      }
    }
    vTaskDelay(200 / portTICK_PERIOD_MS);
  }
}

void setup() {
  Serial.begin(115200);
  setupWiFi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(mqttCallback);

  // Setup pins for IR sensors
  for (int i = 0; i < numSensors; i++) {
    pinMode(irPins[i], INPUT);
  }
  
  // Setup servo
  servo.attach(servoPin);
  servo.write(0);

  // Create FreeRTOS tasks
  xTaskCreate(mqttTask, "MQTT Task", 4096, NULL, 1, NULL);
  xTaskCreate(irSensorTask, "IR Sensor Task", 4096, NULL, 1, NULL);
  xTaskCreate(servoTask, "Servo Task", 4096, NULL, 1, NULL);
}

void loop() {
  vTaskDelete(NULL); // ไม่ใช้ loop() เพราะใช้ FreeRTOS
}
