/*
  Cloudlight Receiver for ESP8266
  Tested on Wemos D1 Mini

  Gets IR signal and sends to cloudlight for control via IR Remote
  Also connects to Internet for eventual web and MQTT control

  Work very much in progress!

  Inspired by Cloud Mood Lamp By James Bruce / http://www.makeuseof.com/

  Used to get around the limitations of having two libraries that both require exact timings to work right!

  I2C on Wemos - C is D1 / D is D2

  Wemos Pins:  https://steve.fi/hardware/d1-pins/
*/

#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include <Arduino.h>
#include <ESP8266WiFi.h>          //ESP8266 Core WiFi Library
#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#include <IRremoteESP8266.h>      // IR library for ESP8266
#include <IRrecv.h>               // IR Library receiver
#include <IRutils.h>              // IR Library tools
#include <Wire.h>                 // Wire library for I2C
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
#include <WiFiUdp.h>              // For OTA Updates
#include <ArduinoOTA.h>           // For OTA Updates
#include <DoubleResetDetector.h>  // https://github.com/datacute/DoubleResetDetector/


//for LED status
#include <Ticker.h>
Ticker ticker;

// Number of seconds after reset during which a 
// subseqent reset will be considered a double reset.
#define DRD_TIMEOUT 10

// RTC Memory Address for the DoubleResetDetector to use
#define DRD_ADDRESS 0

DoubleResetDetector drd(DRD_TIMEOUT, DRD_ADDRESS);

int RECV_PIN = 0; // Wemos Pin D3 = Arduino pin 0

IRrecv irrecv(RECV_PIN);

decode_results results;

char default_hostName[40] = "cloudlamp";
char default_mqtt_server[40] = "";
char default_mqtt_login[40] = "";
char default_mqtt_password[40] = "";

char hostName[40] = "";
char mqtt_server[40] = "";
char mqtt_login[40] = "";
char mqtt_password[40] = "";

//flag for saving data
bool shouldSaveConfig = false;

void setup()
{

  pinMode(2, OUTPUT); // LED
  pinMode(RECV_PIN, INPUT); // set input for LED
  Wire.begin(); // Start I2C Bus as Master
  Serial.begin(115200);
  irrecv.enableIRIn(); // Start the receiver
  while (!Serial)  // Wait for the serial connection to be establised.
    delay(50);
  Serial.println();
  Serial.print("IRrecv is now running and waiting for IR message on Pin ");
  Serial.println(RECV_PIN);

  //clean FS, for testing
  //SPIFFS.format();

  //read configuration from FS json
  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    readSettingsFromJSON();
  } else {
    Serial.println("failed to mount FS");
  }

  // WiFi
  WiFiManagerParameter custom_hostname("hostName", "Hostname", hostName, 40);
  WiFiManagerParameter custom_mqtt_server("mqtt_server", "MQTT Server", mqtt_server, 40);
  WiFiManagerParameter custom_mqtt_login("mqtt_login", "MQTT Login", mqtt_login, 40);
  WiFiManagerParameter custom_mqtt_password("mqtt_password", "MQTT Password", mqtt_password, 40);
  WiFiManager wifiManager;  // initialize WiFiManager
  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //add all your parameters here
  wifiManager.addParameter(&custom_hostname);
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_login);
  wifiManager.addParameter(&custom_mqtt_password);

  wifiManager.setConfigPortalTimeout(90); // goes on if no setup after 3 mins

  if (drd.detectDoubleReset()) {
    Serial.println("Double Reset Detected - Resetting WiFi");
    //reset settings - for testing
    wifiManager.resetSettings();
  }

  //Set hostname
  WiFi.hostname(hostName);
  //reset settings - for testing
  //wifiManager.resetSettings();
  //wifiManager.autoConnect("cloudlamp");
  if (!wifiManager.autoConnect(hostName)) {
    Serial.println("failed to connect and hit timeout");
    //reset and try again, or maybe put it to deep sleep
    //ESP.reset();
    //delay(1000);
  } else {
    //if you get here you have connected to the WiFi
    Serial.println("connected to WiFi :)");
  }


  if (shouldSaveConfig) {
    strcpy(hostName, custom_hostname.getValue());
    strcpy(mqtt_server, custom_mqtt_server.getValue());
    strcpy(mqtt_login, custom_mqtt_login.getValue());
    strcpy(mqtt_password, custom_mqtt_password.getValue());
    //writeSettingsToEEPROM();//writeSettingsToEEPROM();
    saveSettingsToJSON(); // save config
  }
  ticker.detach();

  //OTA Updates Enabled 
  
  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
}

void loop() {
  if (irrecv.decode(&results)) {

    if (results.value != 0xFFFFFFFF) {
      serialPrintUint64(results.value, HEX);
      Serial.println("");
      //only transmit if its not a repeat value. theyre useless. you may need to
      // adjust for your own remote repeat values
      Wire.beginTransmission(9);
      union {
        uint64_t num;
        uint8_t parts[8];
      } u;
      u.num = results.value;
      //for (int x=0; x<8; x++) {
      //  Wire.write (u.parts[x]);
      //}
      Wire.write (u.parts[0]);
      Wire.endTransmission();
      digitalWrite(2, LOW);
      delay(250);
      digitalWrite(2, HIGH);
      //delay(250);
    }
    irrecv.enableIRIn();
    irrecv.resume(); // Receive the next value
  }
  ArduinoOTA.handle();

  // double reset detector
  drd.loop();
}

//gets called when WiFiManager enters configuration mode
void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
  //entered config mode, make led toggle faster
  ticker.attach(0.2, tick);
}

void tick()
{
  //toggle state
  int state = digitalRead(BUILTIN_LED);  // get the current state of GPIO1 pin
  digitalWrite(BUILTIN_LED, !state);     // set pin to the opposite state
}

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void readSettingsFromJSON() {
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

        strcpy(hostName, json["hostName"]);
        strcpy(mqtt_server, json["mqtt_server"]);
        strcpy(mqtt_login, json["mqtt_login"]);
        strcpy(mqtt_password, json["mqtt_password"]);

        Serial.print("Hostname: ");
        Serial.println(hostName);
        Serial.print("MQTT Server: ");
        Serial.println(mqtt_server);
        Serial.print("MQTT Login: ");
        Serial.println(mqtt_login);
        Serial.print("MQTT Password: ");
        Serial.println(mqtt_password);
      } else {
        Serial.println("failed to load json config");
      }
      configFile.close();
    }
  } else {
    Serial.println("Config file not found - setting defaults");
    strcpy(hostName, "cloudlight");
    strcpy(mqtt_server, "");
    strcpy(mqtt_login, "");
    strcpy(mqtt_password, "");
    saveSettingsToJSON();
  }
}

void saveSettingsToJSON() {
  Serial.println("saving config");
  DynamicJsonBuffer jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();
  json["hostName"] = hostName;
  json["mqtt_server"] = mqtt_server;
  json["mqtt_login"] = mqtt_login;
  json["mqtt_password"] = mqtt_password;

  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile) {
    Serial.println("failed to open config file for writing");
  }

  json.printTo(Serial);
  json.printTo(configFile);
  configFile.close();
  //end save
}
