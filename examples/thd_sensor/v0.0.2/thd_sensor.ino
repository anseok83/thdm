// Copyright(c) 2016 Anseok Lee (anseok83_at_gmail_dot_com)
// MIT License
//
// THD (Temperature, humidity and dust density) Sensor
// https://github.com/anseok83/thdm

// Version 0.0.2 (2016-05-21)

// Windowing length
#define DUST_WINDOW_SIZE  5
#define TEMP_WINDOW_SIZE  5
#define INITIAL_UDPATE_SKIP  5

// DHT-11 (Temperature and humidity sensor)
#include "DHT.h"
#define DHTPIN 4     // what digital pin we're connected to
#define DHTTYPE DHT11   // DHT 11

// ESP8266 (WiFi module) and socket.io
#include <ESP8266WiFi.h>
#include <SocketIOClient.h>
#include <ArduinoJson.h>

// Global variables for networking (Wifi, socket.io)
StaticJsonBuffer<200> jsonBuffer;
SocketIOClient client;

const char* ssid     = "YOUR_SSID";           // Wifi SSID
const char* password = "YOUR_PASSWORD";      // Wifi password

char host[] = "YOUR_SERVER_ADDRESS";    // Address of THD Server
int port = YOUR_PORT_NUMBER;                          // Port of THD Server
//char host[] = "192.168.1.10";
//int port = 3000;
extern String RID;
extern String Rname;
extern String Rcontent;

// Reporting interval
unsigned long previousMillis = 0;
long interval = 5000;
unsigned long lastreply = 0;
unsigned long lastsend = 0;
unsigned long updateCount = 0;
String JSON;
JsonObject& root = jsonBuffer.createObject();

// Ethernet address
uint8_t  MAC_STA[] = {0, 0, 0, 0, 0, 0};
char MacAddrStr[] = "000000000000";

// Global variables for sharp dust sensor
int dustPower = 5;
int dustMeasure = 0;

int samplingTime = 280;
int deltaTime = 40;
int sleepTime = 9680;

float voMeasured = 0;
float calcVoltage = 0;
float dustDensity = 0;

int i;
float dustValue[DUST_WINDOW_SIZE];
int dustValueIndex = 0;
float dustValueSum, dustValueAvg = 0;
int dustRapidChange = 0;
int lastDustValueIndex = 0;

// Global variables for DHT-11
DHT dht(DHTPIN, DHTTYPE);

float tempValue[TEMP_WINDOW_SIZE];
float humidValue[TEMP_WINDOW_SIZE];
int tempValueIndex = 0;
float tempValueSum, tempValueAvg = 0;
float humidValueSum, humidValueAvg = 9;


void setup() {
  // Setup
  Serial.begin(115200);
  delay(1);

  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  // Ethernet address lookup
  uint8_t* MAC = WiFi.macAddress(MAC_STA);
  for (int i = 0; i < 6; i++) {
    Serial.print(":");
    Serial.print(MAC[i], HEX);
  }
  Serial.println("");
  sprintf(MacAddrStr, "%2X%2X%2X%2X%2X%2X", MAC[0], MAC[1], MAC[2], MAC[3], MAC[4], MAC[5]);
  Serial.print("MacAddrToStr: ");
  Serial.println(MacAddrStr);

  // Setup IO pins
  pinMode(dustPower, OUTPUT);

  // DHT sensor initialization
  dht.begin();
}

void loop() {
  // Start measurement

  // Dust sensor
  digitalWrite(dustPower, LOW); // power on the LED
  delayMicroseconds(samplingTime);

  voMeasured = analogRead(dustMeasure); // read the dust value

  delayMicroseconds(deltaTime);
  digitalWrite(dustPower, HIGH); // turn the LED off
  delayMicroseconds(sleepTime);

  // 0 - 3.3V mapped to 0 - 1023 integer values
  calcVoltage = voMeasured * (3.3 / 1024);
  dustDensity = (0.172 * calcVoltage) * 1000;

  // Dust sensor windowing & averaging
  dustValue[dustValueIndex] = dustDensity;
  lastDustValueIndex = dustValueIndex;
  dustValueIndex = (dustValueIndex + 1) % DUST_WINDOW_SIZE;

  dustValueSum = 0;
  for (i = 0; i < DUST_WINDOW_SIZE; i++) {
    dustValueSum += dustValue[i];
  }

  dustValueAvg = (dustValueSum / DUST_WINDOW_SIZE);

  // Log (instantaneous)
  Serial.print("Raw Signal Value (0-1023): ");
  Serial.print(voMeasured);

  Serial.print(" - Voltage: ");
  Serial.print(calcVoltage);

  Serial.print(" - Dust Density [ug/m3]: ");
  Serial.println(dustDensity);

  // Temp. and humidity sensors

  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();
  // Read temperature as Fahrenheit (isFahrenheit = true)
  float f = dht.readTemperature(true);

  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t) || isnan(f)) {
    Serial.println("Failed to read from DHT sensor!");
    //return;
  }

  // Temp & humid sensor windowing & averaging
  tempValue[tempValueIndex] = t;
  humidValue[tempValueIndex] = h + 5;   // Adjustment (by anseok)
  tempValueIndex = (tempValueIndex + 1) % TEMP_WINDOW_SIZE;

  tempValueSum = 0;
  humidValueSum = 0;
  for (i = 0; i < TEMP_WINDOW_SIZE; i++) {
    tempValueSum += tempValue[i];
    humidValueSum += humidValue[i];
  }

  // Averaging
  tempValueAvg = (tempValueSum / TEMP_WINDOW_SIZE);
  humidValueAvg = (humidValueSum / TEMP_WINDOW_SIZE);

  // Log (instantaneous)
  Serial.print("Temp. & Hum. sample OK: ");
  Serial.print(t);
  Serial.print(" *C, ");
  Serial.print(h);
  Serial.println(" %");

  // Send data to THD Server
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis > interval)
  {
    previousMillis = currentMillis;

    // Check the connectivity
    // Wifi
    if (WiFi.status() != WL_CONNECTED)
    {
      Serial.println("Wifi connection disconnected!");
      WiFi.begin(ssid, password);

      while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
      }
      Serial.println("");
      Serial.println("WiFi connected");
      Serial.println("IP address: ");
      Serial.println(WiFi.localIP());
    }

    // Socket.IO part
    if (!client.connected())
    {
      // Reconnect
      if (!client.connect(host, port)) {
        Serial.println("connection failed");

        // At least sleep 1 sec.
        delay(1000);
        return;
      }
    }

    // Initial update delaying (for stabilizing sensor data)
    if (updateCount < INITIAL_UDPATE_SKIP)
    {
      updateCount++;

      // At least sleep 1 sec.
      delay(1000);
      return;
    }

    Serial.println("Send sensor data to server");

    // JSON object to report sensor data
    JSON = "";
    root["temp"] = tempValueAvg;
    root["humid"] = humidValueAvg;
    root["dust"] = dustValueAvg;
    root["eth"] = MacAddrStr;
    root.printTo(JSON);

    Serial.println(JSON);
    client.sendJSON("thd", JSON);         // socket.io event for THDM

    // Disconnect the connection
    client.disconnect();

    lastsend = millis();
  }

  // Sleep 1 sec
  delay(1000);
}


