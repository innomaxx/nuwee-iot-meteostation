
#include <DHT.h>
#include <string.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <BlynkSimpleEsp8266.h>

#define DHTPIN 12               // D6
#define LIGHTPIN A0             // A0
#define SHOCKPIN 13             // D7
#define SHOCK_LED_RED_PIN 4     // D1
#define SHOCK_LED_GREEN_PIN 5   // D2

#define DHTTYPE DHT11
#define BLYNK_PRINT Serial
#define NMON_POST_INTERVAL 300000 // 5 min

char ssid[] = ""; // Wi-Fi AP SSID
char pass[] = ""; // Wi-Fi AP password
char auth[] = ""; // Blynk auth token

char deviceMAC[] = "";                    // ESP device MAC
const char* mqttServer = "narodmon.com";  // Narodmon server
const int mqttPort = 1883;                // Narodmon port
const char* mqttUser = "";                // Narodmon login
const char* mqttPassword = "";            // Narodmon password

String _mqttTopicBasic = String(mqttUser) + String("/MySensor/");
String _mqttTopic = _mqttTopicBasic + String("status");
char* mqttTopic = (char*) _mqttTopic.c_str();
String _mqttTopicPublish = _mqttTopicBasic + String("json");
char* mqttTopicPublish = (char*) _mqttTopicPublish.c_str();

DHT dht(DHTPIN, DHTTYPE);
BlynkTimer timer;
WiFiClient espClient;
PubSubClient client(espClient);
StaticJsonDocument<200> nmonResponse;
unsigned long lastConnectionTime = 0;

void sendSensor()
{
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  int light = analogRead(LIGHTPIN);

  if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  /*
  Serial.print("Temperature: ");
  Serial.print(t);
  Serial.print(" °C | Humidity: ");
  Serial.print(h);
  Serial.println(" %");
  Serial.print("Light intensity: ");
  Serial.print(light);
  Serial.println(" lm");
  */

  Blynk.virtualWrite(V5, h);
  Blynk.virtualWrite(V6, t);
  Blynk.virtualWrite(V7, light);

  unsigned long elapsedFromLastTransfer = millis() - lastConnectionTime;

  /*
  Serial.print("Elapsed:" );
  Serial.println(elapsedFromLastTransfer);
  Serial.print("Last conn time: ");
  Serial.println(lastConnectionTime);
  */

  if (elapsedFromLastTransfer > NMON_POST_INTERVAL) {
    if (WiFi.status() == WL_CONNECTED) {
      if (SendToNarodMon(t, h, light)) {
        lastConnectionTime = millis();
        Serial.println("Data sent");
      } else {
        lastConnectionTime = millis() - NMON_POST_INTERVAL + 15000;
        Serial.println("Not connected to WiFi"); // next attempt in 15s
      }
    }
  }
}

boolean SendToNarodMon(float temp, float hum, int light) {
  nmonResponse["temp"] = temp;
  nmonResponse["humidity"] = hum;
  nmonResponse["light"] = light;

  String jsonString;
  serializeJson(nmonResponse, jsonString);
  char* textResponse = (char*) jsonString.c_str();
  Serial.print("JSON Data: ");
  Serial.println(textResponse);

  boolean isPublished = client.publish(mqttTopicPublish, textResponse);
  if (isPublished) {
    Serial.println("Narodmon: measurements published");
  } else {
    Serial.println("Narodmon: failed to publish measurements");
  }
}

void setup()
{
  pinMode(SHOCKPIN, INPUT);
  pinMode(SHOCK_LED_RED_PIN, OUTPUT);
  pinMode(SHOCK_LED_GREEN_PIN, OUTPUT);

  Serial.begin(9600);
  Blynk.begin(auth, ssid, pass);

  Serial.println("Connecting to WiFi (1)...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting to WiFi (2)...");
  }
  Serial.println("Connected to the WiFi network");

  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);

  Serial.print("Topic connect: ");
  Serial.println(mqttTopic);
  Serial.print("Topic publish: ");
  Serial.println(mqttTopicPublish);

  while (!client.connected()) {
    Serial.println("Connecting to Narodmon...");

    boolean isConnected = client.connect(
      deviceMAC, mqttUser, mqttPassword,    // id, user, pass
      mqttTopic,                            // topic
      0, 0, "online"                        // willQos, willRetain, message
    );

    if (isConnected) {
      Serial.println("Connected");
    } else {
      Serial.print("failed with state ");
      Serial.print(client.state());
      delay(2000);
    }
  }

  nmonResponse["temp"] = 0.0;
  nmonResponse["humidity"] = 0.0;
  nmonResponse["light"] = 0;

  dht.begin();

  timer.setInterval(1000L, sendSensor);
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived in topic: ");
  Serial.println(topic);

  Serial.print("Message:");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }

  Serial.println();
  Serial.println("-----------------------");
}

void loop()
{
  Blynk.run();
  timer.run();
  client.loop();

  int shock = digitalRead(SHOCKPIN);
  if (shock == HIGH) {
    digitalWrite(SHOCK_LED_GREEN_PIN, HIGH);
    digitalWrite(SHOCK_LED_RED_PIN, LOW);
  } else {
    digitalWrite(SHOCK_LED_GREEN_PIN, LOW);
    digitalWrite(SHOCK_LED_RED_PIN, HIGH);
    delay(50);
    Blynk.virtualWrite(V8, 255);
    delay(300);
    Blynk.virtualWrite(V8, 0);
    Serial.println("Shock detected");
  }
}
