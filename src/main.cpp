/*******************************************************************
MQTT Infrared
Webupdate per //ip/update -> bin File muss davor erzeugt werden
*******************************************************************/
volatile int interruptCounter; // counter use to detect hall sensor in fan
int RPM;                       // variable used to store computed value of fan RPM
unsigned long previousmills;

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <PubSubClient.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ESP8266HTTPUpdateServer.h>

#include "wifisecrets.h"

#ifndef UNIT_TEST
#endif
#include <Arduino.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>

const char *host = "Test1Ventilator";
const char *ssid = STASSID;
const char *password = STAPSK;

ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;

// NODEMCU ESP8266
#define tachInputPIN D6
#define calculationPeriod 1000 // Number of milliseconds over which to interrupts

void ICACHE_RAM_ATTR handleInterrupt()
{ // This is the function called by the interrupt
  interruptCounter++;
}

#define fanspeed "Vornado/Test1Ventilator/fanspeed"

const uint16_t kIrLed = 4; // ESP8266 GPIO pin to use. Recommended: 4 (D2).

IRsend irsend(kIrLed); // Set the GPIO to be used to sending the message.

// IR SIGNAL Vornado Power
uint16_t rawDataONOFF[167] = {1248, 422, 1278, 368, 452, 1192, 1300, 370, 1280, 366, 450, 1222, 454, 1190, 454, 1192, 476, 1194, 454, 1190, 426, 1244, 1278, 7256, 1302, 368, 1278, 366, 424, 1244, 1278, 366, 1278, 366, 476, 1192, 454, 1190, 424, 1246, 454, 1190, 454, 1192, 476, 1190, 1278, 7278, 1278, 366, 1300, 368, 454, 1190, 1272, 398, 1278, 366, 454, 1190, 476, 1192, 454, 1190, 424, 1246, 454, 1190, 454, 1190, 1304, 7280, 1278, 366, 1276, 394, 454, 1190, 1280, 366, 1298, 370, 456, 1190, 454, 1216, 456, 1188, 454, 1190, 474, 1196, 454, 1188, 1248, 7336, 1280, 364, 1280, 366, 478, 1192, 1278, 366, 1272, 398, 454, 1188, 456, 1188, 478, 1192, 454, 1190, 448, 1222, 454, 1190, 1278, 7304, 1280, 366, 1278, 366, 476, 1194, 1278, 366, 1272, 398, 454, 1190, 454, 1190, 478, 1192, 454, 1190, 446, 1224, 454, 1190, 1278, 7278, 1278, 366, 1298, 372, 454, 1190, 1278, 394, 1276, 366, 454, 1190, 474, 1196, 454, 1190, 454, 1216, 454, 1190, 454, 1190, 1298}; // SYMPHONY D81
// IR SIGNAL Vornado Plustaste
uint16_t rawDataPlus[191] = {1250, 422, 1250, 396, 424, 1220, 1276, 394, 1252, 392, 424, 1246, 450, 1194, 428, 1218, 450, 1220, 426, 1218, 1248, 422, 426, 8110, 1276, 394, 1250, 394, 424, 1248, 1274, 370, 1250, 396, 448, 1220, 448, 1196, 424, 1246, 450, 1196, 448, 1198, 1274, 394, 450, 8110, 1250, 396, 1276, 394, 424, 1220, 1248, 422, 1250, 396, 424, 1222, 448, 1220, 424, 1220, 422, 1248, 424, 1220, 1250, 420, 424, 8134, 1248, 396, 1248, 422, 424, 1220, 1274, 372, 1274, 398, 422, 1220, 424, 1248, 422, 1220, 424, 1222, 448, 1222, 1250, 396, 424, 8160, 1274, 372, 1248, 422, 424, 1220, 1248, 396, 1270, 400, 424, 1222, 424, 1222, 448, 1220, 422, 1222, 422, 1248, 1272, 374, 422, 8166, 1248, 398, 1248, 422, 424, 1224, 1248, 398, 1268, 402, 422, 1222, 422, 1224, 446, 1222, 422, 1224, 420, 1248, 1248, 398, 422, 8138, 1272, 398, 1248, 398, 422, 1250, 1248, 396, 1248, 422, 422, 1222, 422, 1222, 446, 1224, 424, 1220, 422, 1248, 1250, 396, 422, 8138, 1248, 398, 1272, 398, 424, 1222, 1248, 424, 1248, 398, 422, 1222, 446, 1224, 422, 1222, 422, 1248, 422, 1222, 1248, 398, 444}; // SYMPHONY D82
// IR SIGNAL Vornado Minustaste
uint16_t rawDataMinus[143] = {1272, 400, 1278, 366, 454, 1190, 1304, 366, 1280, 364, 446, 1224, 456, 1188, 458, 1188, 480, 1190, 1280, 366, 446, 1224, 454, 8080, 1306, 364, 1280, 366, 446, 1222, 1278, 366, 1278, 366, 482, 1188, 456, 1188, 446, 1222, 456, 1190, 1278, 366, 480, 1188, 454, 8102, 1280, 366, 1304, 366, 454, 1190, 1272, 398, 1278, 366, 456, 1188, 480, 1190, 454, 1190, 448, 1222, 1278, 366, 456, 1188, 480, 8102, 1278, 366, 1272, 398, 454, 1190, 1278, 366, 1304, 366, 456, 1190, 444, 1224, 456, 1188, 456, 1190, 1304, 366, 456, 1190, 448, 8110, 1304, 366, 1278, 394, 452, 1190, 1278, 366, 1298, 372, 456, 1188, 456, 1214, 456, 1190, 454, 1190, 1298, 370, 454, 1190, 454, 8128, 1280, 366, 1278, 394, 454, 1188, 1280, 366, 1298, 372, 454, 1190, 454, 1216, 454, 1190, 454, 1190, 1300, 372, 454, 1190, 454}; // SYMPHONY D84
// IR SIGNAL Vornado Uhrtaste
uint16_t rawDataUhr[167] = {1272, 398, 1280, 364, 456, 1190, 1304, 366, 1280, 366, 446, 1224, 456, 1188, 1280, 366, 480, 1188, 454, 1190, 446, 1222, 456, 8076, 1304, 364, 1280, 366, 446, 1222, 1280, 364, 1278, 366, 480, 1188, 456, 1190, 1298, 372, 454, 1188, 456, 1214, 456, 1188, 456, 8102, 1280, 364, 1306, 364, 456, 1188, 1298, 372, 1280, 364, 456, 1190, 482, 1188, 1280, 366, 448, 1222, 456, 1188, 456, 1214, 456, 8102, 1280, 366, 1270, 400, 456, 1188, 1280, 366, 1304, 364, 456, 1190, 422, 1246, 1280, 366, 456, 1188, 480, 1190, 456, 1188, 424, 8160, 1280, 364, 1278, 392, 456, 1188, 1280, 364, 1300, 370, 456, 1188, 456, 1214, 1280, 364, 456, 1190, 474, 1196, 456, 1190, 454, 8132, 1280, 364, 1280, 366, 482, 1188, 1280, 366, 1296, 372, 456, 1188, 456, 1214, 1280, 364, 456, 1188, 448, 1220, 456, 1188, 456, 8126, 1280, 366, 1280, 366, 448, 1222, 1280, 366, 1248, 422, 456, 1188, 456, 1190, 1300, 370, 454, 1190, 452, 1218, 454, 1190, 456}; // SYMPHONY D90

const char *mqtt_server = "10.17.50.36"; // DIES IST EIN ÖFFENTLICHER MQTT BROKER

WiFiClient espClient;
PubSubClient client(espClient);
void setup_wifi()
{
  delay(100);

  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.hostname(host);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  randomSeed(micros());
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Command from MQTT broker is : ");
  Serial.print(topic);

  Serial.println();
  Serial.print(" publish data is:");

  char *payload_str;
  payload_str = (char *)malloc(length + 1);
  memcpy(payload_str, payload, length);
  payload_str[length] = '\0';

  if (String(topic) == "Vornado/Test1Ventilator")
  {
    Serial.print((String)payload_str);

    // Luefter AN/AUS
    if (String(payload_str) == "power")
    {

      irsend.sendRaw(rawDataONOFF, 167, 0.006);
    }
    // Luefterstufe plus
    if (String(payload_str) == "plus")
    {

      irsend.sendRaw(rawDataPlus, 191, 0.006);
    }
    // Luefterstufe minus
    if (String(payload_str) == "minus")
    {

      irsend.sendRaw(rawDataMinus, 143, 0.006);
    }
    // Lueftertimer
    if (String(payload_str) == "uhr")
    {

      irsend.sendRaw(rawDataUhr, 167, 0.006);
    }
  }
}

void reconnect()
{

  while (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");

    String clientId = "YOU_CHOOSE_id";
    clientId += String(random(0xffff), HEX);

    if (client.connect(clientId.c_str()))
    {
      Serial.println("connected");

      client.subscribe("Vornado/Test1Ventilator");
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(6000);
    }
  }
}

void setup()
{
  irsend.begin();
  Serial.begin(9600);

  setup_wifi();
  client.setServer(mqtt_server, 1883);

  previousmills = 0;
  interruptCounter = 0;
  RPM = 0;

  // NODEMCU esp8266

  pinMode(tachInputPIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(tachInputPIN), handleInterrupt, FALLING);

  MDNS.begin(host);

  httpUpdater.setup(&httpServer);
  httpServer.begin();

  MDNS.addService("http", "tcp", 80);
  Serial.printf("HTTPUpdateServer ready! Open http://%s.local/update in your browser\n", host);
}

void computeFanSpeed(int count)
{
  // interruptCounter counts 1 pulses per revolution of the fan over a one second period
  RPM = count / 1;
}

void displayFanSpeed()
{
  Serial.print(RPM);        // Prints the computed fan speed to the serial monitor
  Serial.print(" RPM\r\n"); // Prints " RPM" and a new line to the serial monitor
  client.publish(fanspeed, String(RPM).c_str(), true);
}

void loop()
{

  httpServer.handleClient();
  MDNS.update();

  if (!client.connected())
  {
    reconnect();
  }

  client.setCallback(callback);
  client.loop();

  if ((millis() - previousmills) > calculationPeriod)
  { // Process counters once every second
    previousmills = millis();
    int count = interruptCounter;
    interruptCounter = 0;
    computeFanSpeed(count);
    displayFanSpeed();
  }
  yield();
}

