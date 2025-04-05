#include <WiFi.h>
#include <PubSubClient.h>

// WiFi Credentials
const char* ssid = "Khim";
const char* password = "123456789";

// MQTT Broker
const char* mqtt_server = "test.mosquitto.org";

// MQTT Client
WiFiClient espClient;
PubSubClient client(espClient);

// IR Sensor pins for object detection (10 sensors)
const int irPins[] = {32, 14, 34, 15, 22, 18, 16, 17, 27, 13}; // IR Sensors
const int numSensors = sizeof(irPins) / sizeof(irPins[0]);

// MQTT Topics for sensors 99 - 108
String topics[] = {
  "kmitl/project/irsensor/99",
  "kmitl/project/irsensor/100",
  "kmitl/project/irsensor/101",
  "kmitl/project/irsensor/102",
  "kmitl/project/irsensor/103",
  "kmitl/project/irsensor/104",
  "kmitl/project/irsensor/105",
  "kmitl/project/irsensor/106",
  "kmitl/project/irsensor/107",
  "kmitl/project/irsensor/108"
};

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
      Serial.printf("IR Sensor %d (topic %s): %s\n", i + 1, topics[i].c_str(), (irState == LOW ? "Object detected (1)" : "No object (0)"));
      client.publish(topics[i].c_str(), message.c_str());
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
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

  // Create FreeRTOS tasks
  xTaskCreate(mqttTask, "MQTT Task", 4096, NULL, 1, NULL);
  xTaskCreate(irSensorTask, "IR Sensor Task", 4096, NULL, 1, NULL);
}

void loop() {
  vTaskDelete(NULL); // ไม่ใช้ loop() เพราะใช้ FreeRTOS
}
