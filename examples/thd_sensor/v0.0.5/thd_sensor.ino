// Copyright(c) 2016 Anseok Lee (anseok83_at_gmail_dot_com)
// MIT License
//
// THD (Temperature, humidity and dust density) Sensor
// https://github.com/anseok83/thdm

// Version 0.0.2 (2016-05-21)
// Initial code
// Version 0.0.3 (2016-06-24)
// Modify dust sensor data retrieving
// Adding Wifi manager
// Version 0.0.4 (2016-07-08)
// Standalone operation (with google chart)
// LED status monitoring
// Version 0.0.5 (2016-07-15)
// To make configurable parameters: LED thresholds, measurement/report intervals
// WifiManager: parameter configuration windows (description displays)


//this needs to be first, or it all crashes and burns...
#include <FS.h>


///////////////////////////////////////////////
// Network device
///////////////////////////////////////////////

// ESP8266 (WiFi module)
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include <DNSServer.h>
#include <WiFiUdp.h>
#include "time_ntp.h"
#include <SocketIOClient.h>

// ntp timestamp
unsigned long ulSecs2000_timer = 0;

// Create an instance of the server on Port 80
WiFiServer server(4000);

// Global variables for networking (Wifi, socket.io)
StaticJsonBuffer<200> jsonBuffer;

// Wifi manager
WiFiManager wifiManager;

char measurement_interval[6] = "10";
int measurement_interval_ms = 10000;

//flag for saving data
bool shouldSaveConfig = false;

char ssid[20] = "";
char password[20] = "";

String JSON;
JsonObject& root = jsonBuffer.createObject();
SocketIOClient client;

// Ethernet address
uint8_t  MAC_STA[] = {0, 0, 0, 0, 0, 0};
//char macaddr[] = "000000000000";
char macaddr[20] = "";
char localip[20] = "";
  
///////////////////////////////////////////////
// Data
///////////////////////////////////////////////

// Storage for Measurements
// keep some mem free; allocate remainder
#define KEEP_MEM_FREE 10240

// Measurements
unsigned long ulMeasCount = 0;  // values already measured
unsigned long ulNoMeasValues = 0; // size of array
unsigned long ulMeasDelta_ms;   // distance to next meas time
unsigned long *pulTime;         // array for time points of measurements
float *pfTemp, *pfHum, *pfDust;          // array for temperature, humidity and dust measurements

unsigned long ulReqcount;       // how often has a valid page been requested
unsigned long ulReconncount;    // how often did we connect to WiFi

// Measurement intervals
unsigned long previousMillis = 0;
unsigned long previousDustMeas = 0;
long intervalDustMeas = 500;
unsigned long previousTHMeas = 0;
long intervalTHMeas = 1000;
bool isFirstUpdateDust = true;
bool isFirstUpdateTH = true;


///////////////////////////////////////////////
// Sensors
///////////////////////////////////////////////

// DHT-11 (Temperature and humidity sensor)
#include "DHT.h"

// PIN for WiFi setup reset
#define TRIGGER_PIN 15

// Pins for dust sensors
#define DUST_POWER_PIN 5
#define DUST_MEASURE_PIN 0

// Pin & type for DHT (temp. & hum.)
#define DHTPIN 4        // what digital pin we're connected to
#define DHTTYPE DHT11   // DHT 11

// Pins for LEDs
#define LED_RED_PIN 14
#define LED_GREEN_PIN 12
#define LED_BLUE_PIN 13

// LED Color threshold
#define HUM_LOW_BELOW 60
#define HUM_MEDIUM_BELOW 75
#define DUST_LOW_BELOW 80
#define DUST_MEDIUM_BELOW 150

// Windowing (averaging)
#define DUST_WINDOW_SIZE  100
#define TH_WINDOW_SIZE  5

// Variables for sharp dust sensors
int dustSamplingTime = 280;
int dustDeltaTime = 40;
int dustSleepTime = 9680;

float voMeasured = 0;
float voMeasured2 = 0, voMeasured1 = 0;
float calcVoltage = 0;
float dustDensity = 0;

float dustValue[DUST_WINDOW_SIZE];
int dustValueIndex = 0;
float dustValueSum, dustValueAvg = 0;
int lastDustValueIndex = 0;

// Variables for DHT-11
DHT dht(DHTPIN, DHTTYPE);

float tempValue[TH_WINDOW_SIZE];
float humidValue[TH_WINDOW_SIZE];
int tempValueIndex = 0;
float tempValueSum, tempValueAvg = 0;
float humidValueSum, humidValueAvg = 0;

// Variables for LEDs
unsigned char redValue = 0;
unsigned char greenValue = 0;
unsigned char blueValue = 0;


//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void setup() {
  ////////////////
  // Setup routine
  ////////////////

  // setup globals
  ulReqcount = 0;
  ulReconncount = 0;

  // setup serial port
  Serial.begin(115200);
  delay(1);

  // setup pins
  pinMode(TRIGGER_PIN, INPUT);

  pinMode(LED_RED_PIN, OUTPUT);
  pinMode(LED_GREEN_PIN, OUTPUT);
  pinMode(LED_BLUE_PIN, OUTPUT);

  //read configuration from FS json
  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");

          strcpy(measurement_interval, json["measurement_interval"]);

        } else {
          Serial.println("failed to load json config");
        }
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read


  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_measurement_interval("interval", "Measurement interval [s]", measurement_interval, 5);

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //add all your parameters here
  //  wifiManager.addParameter(&custom_thd_server);
  wifiManager.addParameter(&custom_measurement_interval);

  //fetches ssid and pass from eeprom and tries to connect
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect("THDSensor") ) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }

  // Ethernet address lookup
  uint8_t* MAC = WiFi.macAddress(MAC_STA);
  for (int i = 0; i < 6; i++) {
    Serial.print(":");
    Serial.print(MAC[i], HEX);
  }
  Serial.println("");
  sprintf(macaddr, "%2X%2X%2X%2X%2X%2X", MAC[0], MAC[1], MAC[2], MAC[3], MAC[4], MAC[5]);
  Serial.print("MacAddrToStr: ");
  Serial.println(macaddr);

  // Local IP
  IPAddress myIp = WiFi.localIP();
  sprintf(localip, "%d.%d.%d.%d", myIp[0], myIp[1], myIp[2], myIp[3]);
  

  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");

  // Start the server
  server.begin();
  Serial.println("Server started");

  ///////////////////////////////
  // connect to NTP and get time
  ///////////////////////////////
  ulSecs2000_timer = getNTPTimestamp();
  Serial.print("Current Time UTC from NTP server: " );
  Serial.println(epoch_to_string_with_timezone(ulSecs2000_timer, 9).c_str() );

  ulSecs2000_timer -= millis() / 1000; // keep distance to millis() counter

  // Measurement interval
  strcpy(measurement_interval, custom_measurement_interval.getValue());
  measurement_interval_ms = atoi(measurement_interval) * 1000;

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["measurement_interval"] = measurement_interval;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
  }


  // Connected Wifi network
  (WiFi.SSID()).toCharArray(ssid, 20);
  (WiFi.psk()).toCharArray(password, 20);


  // allocate ram for data storage
  uint32_t free = system_get_free_heap_size() - KEEP_MEM_FREE;
  ulNoMeasValues = free / (sizeof(float) * 3 + sizeof(unsigned long)); // humidity & temp & dust --> 3 + time
  pulTime = new unsigned long[ulNoMeasValues];
  pfTemp = new float[ulNoMeasValues];
  pfHum = new float[ulNoMeasValues];
  pfDust = new float[ulNoMeasValues];

  if (pulTime == NULL || pfTemp == NULL || pfHum == NULL || pfDust == NULL)
  {
    ulNoMeasValues = 0;
    Serial.println("Error in memory allocation!");
  }
  else
  {
    Serial.print("Allocated storage for ");
    Serial.print(ulNoMeasValues);
    Serial.println(" data points.");

    // Measurement report interval
    float fMeasDelta_sec = measurement_interval_ms;
    ulMeasDelta_ms = ( (unsigned long)(fMeasDelta_sec + 0.5) ) * 1000; // round to full sec
    Serial.print("Measurements will happen each ");
    Serial.print(ulMeasDelta_ms);
    Serial.println(" ms.");
  }

  // Setup IO pins
  pinMode(DUST_POWER_PIN, OUTPUT);

  // DHT sensor initialization
  dht.begin();

  // Try to send the network information to thdinfo server

  // THD info server
  char host[] = "myhost";    // Address of THD Server
  int port = myport;                          // Port of THD Server
  
  if (!client.connected())
  {
    // Connect
    if (!client.connect(host, port)) {
      Serial.println("connection failed");
      
      return;
    }
  }

  // JSON object to report sensor data
  JSON = "";
  root["lip"] = localip;
  root["ssid"] = ssid;
  root["eth"] = macaddr;
  root.printTo(JSON);

  Serial.println(JSON);
  client.sendJSON("thd", JSON);         // socket.io event for THDM

  // Disconnect the connection
  client.disconnect();
}


/////////////////////////////////////
// make html table for measured data
/////////////////////////////////////
unsigned long MakeTable (WiFiClient *pclient, bool bStream)
{
  unsigned long ulLength = 0;

  // here we build a big table.
  // we cannot store this in a string as this will blow the memory
  // thus we count first to get the number of bytes and later on
  // we stream this out
  if (ulMeasCount == 0)
  {
    String sTable = "No data reported.<BR>";
    if (bStream)
    {
      pclient->print(sTable);
    }
    ulLength += sTable.length();
  }
  else
  {
    unsigned long ulEnd;
    if (ulMeasCount > ulNoMeasValues)
    {
      ulEnd = ulMeasCount - ulNoMeasValues;
    }
    else
    {
      ulEnd = 0;
    }

    String sTable;
    sTable = "<table style=\"width:100%\"><tr><th>Time / UTC</th><th>Temp &deg;C</th><th>Hum &#037;</th><th>Dust ug/m^3</th></tr>";
    sTable += "<style>table, th, td {border: 2px solid black; border-collapse: collapse;} th, td {padding: 5px;} th {text-align: left;}</style>";
    for (unsigned long li = ulMeasCount; li > ulEnd; li--)
    {
      unsigned long ulIndex = (li - 1) % ulNoMeasValues;
      sTable += "<tr><td>";
      sTable += epoch_to_string_with_timezone(pulTime[ulIndex], 9).c_str();
      sTable += "</td><td>";
      sTable += pfTemp[ulIndex];
      sTable += "</td><td>";
      sTable += pfHum[ulIndex];
      sTable += "</td><td>";
      sTable += pfDust[ulIndex];
      sTable += "</td></tr>";

      // play out in chunks of 1k
      if (sTable.length() > 1024)
      {
        if (bStream)
        {
          pclient->print(sTable);
          //pclient->write(sTable.c_str(),sTable.length());
        }
        ulLength += sTable.length();
        sTable = "";
      }
    }

    // remaining chunk
    sTable += "</table>";
    ulLength += sTable.length();
    if (bStream)
    {
      pclient->print(sTable);
      //pclient->write(sTable.c_str(),sTable.length());
    }
  }

  return (ulLength);
}


////////////////////////////////////////////////////
// make google chart object table for measured data
////////////////////////////////////////////////////
unsigned long MakeList (WiFiClient *pclient, bool bStream)
{
  unsigned long ulLength = 0;

  // here we build a big list.
  // we cannot store this in a string as this will blow the memory
  // thus we count first to get the number of bytes and later on
  // we stream this out
  if (ulMeasCount > 0)
  {
    unsigned long ulBegin;
    if (ulMeasCount > ulNoMeasValues)
    {
      ulBegin = ulMeasCount - ulNoMeasValues;
    }
    else
    {
      ulBegin = 0;
    }

    String sTable = "";
    for (unsigned long li = ulBegin; li < ulMeasCount; li++)
    {
      // result shall be [time, temp, hum, dust],
      unsigned long ulIndex = li % ulNoMeasValues;
      sTable += "['";
      sTable += epoch_to_time_string_with_timezone(pulTime[ulIndex], 9).c_str();
      sTable += "',";
      sTable += pfTemp[ulIndex];
      sTable += ",";
      sTable += pfHum[ulIndex];
      sTable += ",";
      sTable += pfDust[ulIndex];
      sTable += "],\n";

      // play out in chunks of 1k
      if (sTable.length() > 1024)
      {
        if (bStream)
        {
          pclient->print(sTable);
          //pclient->write(sTable.c_str(),sTable.length());
        }
        ulLength += sTable.length();
        sTable = "";
      }
    }

    // remaining chunk
    if (bStream)
    {
      pclient->print(sTable);
      //pclient->write(sTable.c_str(),sTable.length());
    }
    ulLength += sTable.length();
  }

  return (ulLength);
}


//////////////////////////
// create HTTP 1.1 header
//////////////////////////
String MakeHTTPHeader(unsigned long ulLength)
{
  String sHeader;

  sHeader  = F("HTTP/1.1 200 OK\r\nContent-Length: ");
  sHeader += ulLength;
  sHeader += F("\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n");

  return (sHeader);
}


////////////////////
// make html footer
////////////////////
String MakeHTTPFooter()
{
  String sResponse;

  sResponse  = F("<FONT SIZE=-2><BR>Page view=");
  sResponse += ulReqcount;
  sResponse += F(" - Wifi reconnected=");
  sResponse += ulReconncount;
  sResponse += F(" - Free RAM=");
  sResponse += (uint32_t)system_get_free_heap_size();
  sResponse += F(" - Max. Data points=");
  sResponse += ulNoMeasValues;
  sResponse += F("<BR>Anseok Lee (2016)<BR></body></html>");

  return (sResponse);
}



void loop() {
  // Start measurement

  // is configuration portal requested?
  if ( digitalRead(TRIGGER_PIN) == HIGH ) {
    Serial.print("Reset ESP Board");
    delay(5000);
    wifiManager.resetSettings();
    ESP.reset();
  }

  // current time
  unsigned long currentMillis = millis();

  // Measurement interval (Dust sensor)
  if (currentMillis - previousDustMeas > intervalDustMeas)
  {
    previousDustMeas = currentMillis;

    // Dust sensor
    voMeasured1 = analogRead(DUST_MEASURE_PIN); // read the dust value
    digitalWrite(DUST_POWER_PIN, LOW); // power on the LED
    delayMicroseconds(dustSamplingTime);
    voMeasured2 = analogRead(DUST_MEASURE_PIN); // read the dust value

    voMeasured = voMeasured2 - voMeasured1;

    delayMicroseconds(dustDeltaTime);
    digitalWrite(DUST_POWER_PIN, HIGH); // turn the LED off
    delayMicroseconds(dustSleepTime);

    // 0 - 3.3V mapped to 0 - 1023 integer values
    calcVoltage = voMeasured * (3.3 / 1024);
    // [anseok] 160710 modified

    // original code
//    dustDensity = ((0.172 * calcVoltage) - 0.1) * 1000; // dust density in ug/m^3

    // for sensors type 1
    dustDensity = ((0.172 * calcVoltage)-0.085) * 1000;
    // for sensors type 2
    //dustDensity = ((0.172 * calcVoltage)-0.015) * 1000;

    if (dustDensity < 0)
    {
      dustDensity = 0;
    }

    // Dust sensor windowing & averaging
    dustValue[dustValueIndex] = dustDensity;
    lastDustValueIndex = dustValueIndex;
    dustValueIndex = (dustValueIndex + 1) % DUST_WINDOW_SIZE;

    // only for first update (intialization)
    if (isFirstUpdateDust == true)
    {
      for (int i = 1; i < DUST_WINDOW_SIZE; i++)
      {
        dustValue[i] = dustValue[0];
      }

      isFirstUpdateDust = false;
    }

    // Averaging
    // dust density
    dustValueSum = 0;
    for (int i = 0; i < DUST_WINDOW_SIZE; i++) {
      dustValueSum += dustValue[i];
    }

    dustValueAvg = (dustValueSum / DUST_WINDOW_SIZE);

    //    // Log (instantaneous)
    //    Serial.print("Raw Signal Value (0-1023): ");
    //    Serial.print(voMeasured);
    //
    //    Serial.print(" - Voltage: ");
    //    Serial.print(calcVoltage);
    //
    //    Serial.print(" - Dust Density [ug/m3]: ");
    //    Serial.println(dustDensity);
    //
    //    Serial.print("voMeasured2(");
    //    Serial.print(voMeasured2);
    //    Serial.print(") - voMeasured1(");
    //    Serial.print(voMeasured1);
    //    Serial.print(" = ");
    //    Serial.println(voMeasured);
  }

  // Measurement interval (Dust sensor)
  if (currentMillis - previousTHMeas > intervalTHMeas)
  {
    previousTHMeas = currentMillis;

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
    else {
      // Temp & humid sensor windowing & averaging
      tempValue[tempValueIndex] = t;
      humidValue[tempValueIndex] = h + 2;   // Adjustment (by anseok)
      tempValueIndex = (tempValueIndex + 1) % TH_WINDOW_SIZE;

      // only for first update (intialization)
      if (isFirstUpdateTH == true)
      {
        for (int i = 1; i < TH_WINDOW_SIZE; i++)
        {
          tempValue[i] = tempValue[0];
          humidValue[i] = humidValue[0];
        }

        isFirstUpdateTH = false;
      }
    }

    // Averaging
    // temp & humidity
    tempValueSum = 0;
    humidValueSum = 0;
    for (int i = 0; i < TH_WINDOW_SIZE; i++) {
      tempValueSum += tempValue[i];
      humidValueSum += humidValue[i];
    }

    tempValueAvg = (tempValueSum / TH_WINDOW_SIZE);
    humidValueAvg = (humidValueSum / TH_WINDOW_SIZE);

    //    // Log (instantaneous)
    //    Serial.print("Temp. & Hum. sample OK: ");
    //    Serial.print(t);
    //    Serial.print(" *C, ");
    //    Serial.print(h);
    //    Serial.println(" %");
  }

  // Record measurement data
  if (currentMillis - previousMillis > measurement_interval_ms)
  {
    previousMillis = currentMillis;

    // Stores measurement data
    pfHum[ulMeasCount % ulNoMeasValues] = humidValueAvg;
    pfTemp[ulMeasCount % ulNoMeasValues] = tempValueAvg;
    pfDust[ulMeasCount % ulNoMeasValues] = dustValueAvg;
    pulTime[ulMeasCount % ulNoMeasValues] = millis() / 1000 + ulSecs2000_timer;

    // Log (measurement data)
    Serial.print("Measurement interval[ms]: " );
    Serial.println(measurement_interval_ms);
    //    Serial.print("Logging Temperature: ");
    //    Serial.print(pfTemp[ulMeasCount % ulNoMeasValues]);
    //    Serial.print(" deg - Humidity: ");
    //    Serial.print(pfHum[ulMeasCount % ulNoMeasValues]);
    //    Serial.print("% - DustDensity: ");
    //    Serial.print(pfDust[ulMeasCount % ulNoMeasValues]);
    //    Serial.print("ug/m^3 - Time: ");
    //    Serial.println(pulTime[ulMeasCount % ulNoMeasValues]);

    ulMeasCount++;
  }

  /////////////////////////////////////
  // LED Color update
  /////////////////////////////////////
  if (humidValueAvg < HUM_LOW_BELOW && dustValueAvg < DUST_LOW_BELOW)
  {
    // LL
    redValue = 255;
    greenValue = 255;
    blueValue = 255;
  }
  else if (humidValueAvg < HUM_LOW_BELOW && dustValueAvg < DUST_MEDIUM_BELOW)
  {
    // LM
    redValue = 255;
    greenValue = 140;
    blueValue = 40;
  }
  else if (humidValueAvg < HUM_LOW_BELOW && dustValueAvg >= DUST_MEDIUM_BELOW)
  {
    // LH
    redValue = 255;
    greenValue = 50;
    blueValue = 50;
  }
  else if (humidValueAvg < HUM_MEDIUM_BELOW && dustValueAvg < DUST_LOW_BELOW)
  {
    // ML
    redValue = 160;
    greenValue = 140;
    blueValue = 50;
  }
  else if (humidValueAvg < HUM_MEDIUM_BELOW && dustValueAvg < DUST_MEDIUM_BELOW)
  {
    // MM
    redValue = 100;
    greenValue = 120;
    blueValue = 0;
  }
  else if (humidValueAvg < HUM_MEDIUM_BELOW && dustValueAvg >= DUST_MEDIUM_BELOW)
  {
    // MH
    redValue = 255;
    greenValue = 0;
    blueValue = 0;
  }
  else if (humidValueAvg >= HUM_MEDIUM_BELOW && dustValueAvg < DUST_LOW_BELOW)
  {
    // HL
    redValue = 0;
    greenValue = 120;
    blueValue = 180;
  }
  else if (humidValueAvg >= HUM_MEDIUM_BELOW && dustValueAvg < DUST_MEDIUM_BELOW)
  {
    // HM
    redValue = 0;
    greenValue = 0;
    blueValue = 255;
  }
  else if (humidValueAvg >= HUM_MEDIUM_BELOW && dustValueAvg >= DUST_MEDIUM_BELOW)
  {
    // HH
    redValue = 180;
    greenValue = 0;
    blueValue = 0;
  }

  // Write to LEDs
  analogWrite(LED_RED_PIN, redValue);
  analogWrite(LED_GREEN_PIN, greenValue);
  analogWrite(LED_BLUE_PIN, blueValue);



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

  ///////////////////////////////////
  // Check if a client has connected
  ///////////////////////////////////
  WiFiClient client = server.available();
  if (!client)
  {
    // [anseok] slight delaying
    delay(10);
    return;
  }

  // Wait until the client sends some data
  Serial.println("new client");
  unsigned long ultimeout = millis() + 250;
  while (!client.available() && (millis() < ultimeout) )
  {
    delay(1);
  }
  if (millis() > ultimeout)
  {
    Serial.println("client connection time-out!");
    return;
  }

  /////////////////////////////////////
  // Read the first line of the request
  /////////////////////////////////////
  String sRequest = client.readStringUntil('\r');
  //Serial.println(sRequest);
  client.flush();

  // stop client, if request is empty
  if (sRequest == "")
  {
    Serial.println("empty request! - stopping client");
    client.stop();
    return;
  }

  // get path; end of path is either space or ?
  // Syntax is e.g. GET /?show=1234 HTTP/1.1
  String sPath = "", sParam = "", sCmd = "";
  String sGetstart = "GET ";
  int iStart, iEndSpace, iEndQuest;
  iStart = sRequest.indexOf(sGetstart);
  if (iStart >= 0)
  {
    iStart += +sGetstart.length();
    iEndSpace = sRequest.indexOf(" ", iStart);
    iEndQuest = sRequest.indexOf("?", iStart);

    // are there parameters?
    if (iEndSpace > 0)
    {
      if (iEndQuest > 0)
      {
        // there are parameters
        sPath  = sRequest.substring(iStart, iEndQuest);
        sParam = sRequest.substring(iEndQuest, iEndSpace);
      }
      else
      {
        // NO parameters
        sPath  = sRequest.substring(iStart, iEndSpace);
      }
    }
  }

  ///////////////////////////
  // format the html response
  ///////////////////////////
  String sResponse, sResponse2, sHeader;

  /////////////////////////////
  // format the html page for /
  /////////////////////////////
  if (sPath == "/")
  {
    ulReqcount++;
    int iIndex = (ulMeasCount - 1) % ulNoMeasValues;
    sResponse  = F("<html>\n<head>\n<title>THDM - Air quality monitoring</title>\n<script type=\"text/javascript\" src=\"https://www.google.com/jsapi?autoload={'modules':[{'name':'visualization','version':'1','packages':['gauge']}]}\"></script>\n<script type=\"text/javascript\">\nvar temp=");
    sResponse += pfTemp[iIndex];
    sResponse += F(",hum=");
    sResponse += pfHum[iIndex];
    sResponse += F(",dust=");
    sResponse += pfDust[iIndex];
    sResponse += F(";\ngoogle.load('visualization', '1', {packages: ['gauge']});google.setOnLoadCallback(drawgaugetemp);google.setOnLoadCallback(drawgaugehum);google.setOnLoadCallback(drawgaugedust);\nvar gaugetempOptions = {min: 0, max: 50, yellowFrom: 30, yellowTo: 40,redFrom: 40, redTo: 50, minorTicks: 5};\n");
    sResponse += F("var gaugehumOptions = {min: 0, max: 100, yellowFrom: ");
    sResponse += HUM_LOW_BELOW;
    sResponse += F(", yellowTo: ");
    sResponse += HUM_MEDIUM_BELOW;
    sResponse += F(", redFrom: ");
    sResponse += HUM_MEDIUM_BELOW;
    sResponse += F(", redTo: 100, minorTicks: 5};\n");
    sResponse += F("var gaugedustOptions = {min: 0, max: 500, yellowFrom: ");
    sResponse += DUST_LOW_BELOW;
    sResponse += F(", yellowTo: ");
    sResponse += DUST_MEDIUM_BELOW;
    sResponse += F(", redFrom: ");
    sResponse += DUST_MEDIUM_BELOW;
    sResponse += F(", redTo: 500, minorTicks: 10};\n");
    sResponse += F("var gaugetemp,gaugehum,gaugedust;\n\nfunction drawgaugetemp() {\ngaugetempData = new google.visualization.DataTable();\n");
    sResponse += F("gaugetempData.addColumn('number', 'Temp.');\ngaugetempData.addRows(1);\ngaugetempData.setCell(0, 0, temp);\ngaugetemp = new google.visualization.Gauge(document.getElementById('gaugetemp_div'));\ngaugetemp.draw(gaugetempData, gaugetempOptions);\n}\n\n");
    sResponse += F("function drawgaugehum() {\ngaugehumData = new google.visualization.DataTable();\ngaugehumData.addColumn('number', 'Hum.');\ngaugehumData.addRows(1);\ngaugehumData.setCell(0, 0, hum);\ngaugehum = new google.visualization.Gauge(document.getElementById('gaugehum_div'));\ngaugehum.draw(gaugehumData, gaugehumOptions);\n}\n");
    sResponse += F("function drawgaugedust() {\ngaugedustData = new google.visualization.DataTable();\ngaugedustData.addColumn('number', 'Dust');\ngaugedustData.addRows(1);\ngaugedustData.setCell(0, 0, dust);\ngaugedust = new google.visualization.Gauge(document.getElementById('gaugedust_div'));\ngaugedust.draw(gaugedustData, gaugedustOptions);\n}\n");
    sResponse += F("</script>\n</head>\n<body>\n<font color=\"#000000\"><body bgcolor=\"#ffffff\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=yes\">");
    sResponse += F("<h1>Air quality monitoring</h1>DHT11 and sharp dust sensor<BR><BR><FONT SIZE=+1>Last sensor data at ");
    sResponse += epoch_to_string_with_timezone(pulTime[iIndex], 9).c_str();
    sResponse += F(" UTC<BR>\n");
    sResponse += F("<div id=\"gaugetemp_div\" style=\"float:left; width:160px; height: 160px;\"></div> \n");
    sResponse += F("<div id=\"gaugehum_div\" style=\"float:left; width:160px; height: 160px;\"></div> \n");
    sResponse += F("<div id=\"gaugedust_div\" style=\"float:left; width:160px; height: 160px;\"></div> \n<div style=\"clear:both;\"></div> \n");
    //    sResponse += F("");

    sResponse2 = F("<p>Temperature, humidity and dust density data:<BR><a href=\"/graph\">Graph</a>     <a href=\"/table\">Table</a></p>");
    sResponse2 += MakeHTTPFooter().c_str();

    // Send the response to the client
    client.print(MakeHTTPHeader(sResponse.length() + sResponse2.length()).c_str());
    client.print(sResponse);
    client.print(sResponse2);
  }
  else if (sPath == "/table")
    ////////////////////////////////////
    // format the html page for /table
    ////////////////////////////////////
  {
    ulReqcount++;
    unsigned long ulSizeList = MakeTable(&client, false); // get size of table first

    sResponse  = F("<html><head><title>THDM - Air quality monitoring</title></head><body>");
    sResponse += F("<font color=\"#000000\"><body bgcolor=\"#d0d0f0\">");
    sResponse += F("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=yes\">");
    sResponse += F("<h1>THDM - Air quality monitoring</h1>");
    sResponse += F("<FONT SIZE=+1>");
    sResponse += F("<a href=\"/\">Start page</a><BR><BR>Measurements on every ");
    sResponse += ulMeasDelta_ms;
    sResponse += F("ms<BR>");
    // here the big table will follow later - but let us prepare the end first

    // part 2 of response - after the big table
    sResponse2 = MakeHTTPFooter().c_str();

    // Send the response to the client - delete strings after use to keep mem low
    client.print(MakeHTTPHeader(sResponse.length() + sResponse2.length() + ulSizeList).c_str());
    client.print(sResponse); sResponse = "";
    MakeTable(&client, true);
    client.print(sResponse2);
  }
  else if (sPath == "/graph")
    ///////////////////////////////////
    // format the html page for /graph
    ///////////////////////////////////
  {
    ulReqcount++;
    unsigned long ulSizeList = MakeList(&client, false); // get size of list first

    sResponse  = F("<html>\n<head>\n<title>Air quality monitor sensor</title>\n<script type=\"text/javascript\" src=\"https://www.google.com/jsapi?autoload={'modules':[{'name':'visualization','version':'1','packages':['corechart']}]}\"></script>\n");
    sResponse += F("<script type=\"text/javascript\"> google.setOnLoadCallback(drawChart);\nfunction drawChart() {var data = google.visualization.arrayToDataTable([\n['Zeit / UTC', 'Temp', 'Humidity', 'DustDensity'],\n");
    // here the big list will follow later - but let us prepare the end first

    // part 2 of response - after the big list
    //vAxes:{0:{viewWindowMode:'explicit',gridlines:{color:'black'},format:\"##.##deg\"},1: {gridlines:{color:'transparent'},format:\"##,##%\"},2: {gridlines:{color:'transparent'},format:\"##,##ug/m^3\"}},series:{0:{targetAxisIndex:0},1:{targetAxisIndex:1},2:{targetAxisIndex:2}},
    sResponse2  = F("]);\nvar options = {title: 'THDM', curveType:'function',legend:{ position: 'top'}};");
    sResponse2 += F("var chart = new google.visualization.LineChart(document.getElementById('curve_chart'));chart.draw(data, options);}\n</script>\n</head>\n");
    sResponse2 += F("<body>\n<font color=\"#000000\"><body bgcolor=\"#ffffff\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=yes\"><h1>Air quality monitoring</h1><a href=\"/\">Start page</a><BR>");
    //    sResponse2 += F("<BR>\n<div id=\"curve_chart\" style=\"width: 600px; height: 400px\"></div>");
    sResponse2 += F("<BR>\n<div id=\"curve_chart\" style=\"width: 100%\"></div>");
    sResponse2 += MakeHTTPFooter().c_str();

    // Send the response to the client - delete strings after use to keep mem low
    client.print(MakeHTTPHeader(sResponse.length() + sResponse2.length() + ulSizeList).c_str());
    client.print(sResponse); sResponse = "";
    MakeList(&client, true);
    client.print(sResponse2);
  }
  else
    ////////////////////////////
    // 404 for non-matching path
    ////////////////////////////
  {
    sResponse = "<html><head><title>404 Not Found</title></head><body><h1>Not Found</h1><p>The requested URL was not found on this server.</p></body></html>";

    sHeader  = F("HTTP/1.1 404 Not found\r\nContent-Length: ");
    sHeader += sResponse.length();
    sHeader += F("\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n");

    // Send the response to the client
    client.print(sHeader);
    client.print(sResponse);
  }

  // and stop the client
  client.stop();
  Serial.println("Client disconnected");

}
