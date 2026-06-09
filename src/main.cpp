/*******************************************************************
MQTT Infrared
Webupdate per //ip/update -> bin File muss davor erzeugt werden
*******************************************************************/
volatile int interruptCounter; // counter use to detect hall sensor in fan
int RPM;                       // variable used to store computed value of fan RPM
unsigned long previousmills;
unsigned long lastMqttReconnectAttempt;
unsigned long lastSpeedAdjustAttempt;
unsigned long powerOnGraceUntil;
unsigned long powerOffWaitUntil;
unsigned long lastTelemetryPublish;
unsigned long calibrationNextAction;
unsigned long lastSpeedBurstStep;
int targetSpeedPercent = -1;
int powerToggleAttempts = 0;
int lastPublishedRpm = -1;
int lastPublishedSpeedPercent = -1;
int lastPublishedPowerState = -1;
int calibrationActionCount = 0;
int calibrationRetryCount = 0;
int calibrationStableCount = 0;
int speedBurstRemaining = 0;
int speedBurstDirection = 0;
bool inferredPowerOn = false;

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <PubSubClient.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ESP8266HTTPUpdateServer.h>
#include <EEPROM.h>
#include <stddef.h>

#include "wifisecrets.h"

#ifndef UNIT_TEST
#endif
#include <Arduino.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>

const char *default_device_name = "Test1Ventilator";
const char *firmware_version = "1.0";
const char *ssid = STASSID;
const char *password = STAPSK;
const char *web_user = "admin";
const char *web_password = "vornado";
const char *setup_ap_ssid = "Vornado-Setup";
const char *setup_ap_password = "vornado123";
const unsigned long wifiConnectTimeout = 30000;
const unsigned long speedAdjustPeriod = 1800;
const unsigned long powerOnGracePeriod = 8000;
const unsigned long powerOffRetryPeriod = 15000;
const unsigned long telemetryPublishPeriod = 10000;
const unsigned long calibrationIrGap = 350;
const unsigned long calibrationSettlePeriod = 6000;
const unsigned long speedBurstGap = 300;
const int maxPowerToggleAttempts = 3;
const int maxSpeedBurstSteps = 12;
const int maxCalibrationPoints = 99;
const int calibrationMinDownSteps = 110;
const int calibrationRpmIncreaseThreshold = 3;
const int rpmOffThreshold = 20;
const int rpmPowerOffThreshold = 15;
const int rpmPowerOnThreshold = 40;
const int rpmPercentOne = 79;
const int rpmPercentNinetyNine = 305;
const uint16_t rawIrFrequency = 0;


ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;

// NODEMCU ESP8266
#define tachInputPIN D6
#define calculationPeriod 1000 // Number of milliseconds over which to interrupts

void ICACHE_RAM_ATTR handleInterrupt()
{ // This is the function called by the interrupt
  interruptCounter++;
}

const uint16_t kIrLed = 4; // ESP8266 GPIO pin to use. Recommended: 4 (D2).

IRsend irsend(kIrLed); // Set the GPIO to be used to sending the message.

// IR SIGNAL Vornado Power
const uint16_t rawDataONOFF[167] = {1248, 422, 1278, 368, 452, 1192, 1300, 370, 1280, 366, 450, 1222, 454, 1190, 454, 1192, 476, 1194, 454, 1190, 426, 1244, 1278, 7256, 1302, 368, 1278, 366, 424, 1244, 1278, 366, 1278, 366, 476, 1192, 454, 1190, 424, 1246, 454, 1190, 454, 1192, 476, 1190, 1278, 7278, 1278, 366, 1300, 368, 454, 1190, 1272, 398, 1278, 366, 454, 1190, 476, 1192, 454, 1190, 424, 1246, 454, 1190, 454, 1190, 1304, 7280, 1278, 366, 1276, 394, 454, 1190, 1280, 366, 1298, 370, 456, 1190, 454, 1216, 456, 1188, 454, 1190, 474, 1196, 454, 1188, 1248, 7336, 1280, 364, 1280, 366, 478, 1192, 1278, 366, 1272, 398, 454, 1188, 456, 1188, 478, 1192, 454, 1190, 448, 1222, 454, 1190, 1278, 7304, 1280, 366, 1278, 366, 476, 1194, 1278, 366, 1272, 398, 454, 1190, 454, 1190, 478, 1192, 454, 1190, 446, 1224, 454, 1190, 1278, 7278, 1278, 366, 1298, 372, 454, 1190, 1278, 394, 1276, 366, 454, 1190, 474, 1196, 454, 1190, 454, 1216, 454, 1190, 454, 1190, 1298}; // SYMPHONY D81
// IR SIGNAL Vornado Plustaste
const uint16_t rawDataPlus[191] = {1250, 422, 1250, 396, 424, 1220, 1276, 394, 1252, 392, 424, 1246, 450, 1194, 428, 1218, 450, 1220, 426, 1218, 1248, 422, 426, 8110, 1276, 394, 1250, 394, 424, 1248, 1274, 370, 1250, 396, 448, 1220, 448, 1196, 424, 1246, 450, 1196, 448, 1198, 1274, 394, 450, 8110, 1250, 396, 1276, 394, 424, 1220, 1248, 422, 1250, 396, 424, 1222, 448, 1220, 424, 1220, 422, 1248, 424, 1220, 1250, 420, 424, 8134, 1248, 396, 1248, 422, 424, 1220, 1274, 372, 1274, 398, 422, 1220, 424, 1248, 422, 1220, 424, 1222, 448, 1222, 1250, 396, 424, 8160, 1274, 372, 1248, 422, 424, 1220, 1248, 396, 1270, 400, 424, 1222, 424, 1222, 448, 1220, 422, 1222, 422, 1248, 1272, 374, 422, 8166, 1248, 398, 1248, 422, 424, 1224, 1248, 398, 1268, 402, 422, 1222, 422, 1224, 446, 1222, 422, 1224, 420, 1248, 1248, 398, 422, 8138, 1272, 398, 1248, 398, 422, 1250, 1248, 396, 1248, 422, 422, 1222, 422, 1222, 446, 1224, 424, 1220, 422, 1248, 1250, 396, 422, 8138, 1248, 398, 1272, 398, 424, 1222, 1248, 424, 1248, 398, 422, 1222, 446, 1224, 422, 1222, 422, 1248, 422, 1222, 1248, 398, 444}; // SYMPHONY D82
// IR SIGNAL Vornado Minustaste
const uint16_t rawDataMinus[143] = {1272, 400, 1278, 366, 454, 1190, 1304, 366, 1280, 364, 446, 1224, 456, 1188, 458, 1188, 480, 1190, 1280, 366, 446, 1224, 454, 8080, 1306, 364, 1280, 366, 446, 1222, 1278, 366, 1278, 366, 482, 1188, 456, 1188, 446, 1222, 456, 1190, 1278, 366, 480, 1188, 454, 8102, 1280, 366, 1304, 366, 454, 1190, 1272, 398, 1278, 366, 456, 1188, 480, 1190, 454, 1190, 448, 1222, 1278, 366, 456, 1188, 480, 8102, 1278, 366, 1272, 398, 454, 1190, 1278, 366, 1304, 366, 456, 1190, 444, 1224, 456, 1188, 456, 1190, 1304, 366, 456, 1190, 448, 8110, 1304, 366, 1278, 394, 452, 1190, 1278, 366, 1298, 372, 456, 1188, 456, 1214, 456, 1190, 454, 1190, 1298, 370, 454, 1190, 454, 8128, 1280, 366, 1278, 394, 454, 1188, 1280, 366, 1298, 372, 454, 1190, 454, 1216, 454, 1190, 454, 1190, 1300, 372, 454, 1190, 454}; // SYMPHONY D84
// IR SIGNAL Vornado Uhrtaste
const uint16_t rawDataUhr[167] = {1272, 398, 1280, 364, 456, 1190, 1304, 366, 1280, 366, 446, 1224, 456, 1188, 1280, 366, 480, 1188, 454, 1190, 446, 1222, 456, 8076, 1304, 364, 1280, 366, 446, 1222, 1280, 364, 1278, 366, 480, 1188, 456, 1190, 1298, 372, 454, 1188, 456, 1214, 456, 1188, 456, 8102, 1280, 364, 1306, 364, 456, 1188, 1298, 372, 1280, 364, 456, 1190, 482, 1188, 1280, 366, 448, 1222, 456, 1188, 456, 1214, 456, 8102, 1280, 366, 1270, 400, 456, 1188, 1280, 366, 1304, 364, 456, 1190, 422, 1246, 1280, 366, 456, 1188, 480, 1190, 456, 1188, 424, 8160, 1280, 364, 1278, 392, 456, 1188, 1280, 364, 1300, 370, 456, 1188, 456, 1214, 1280, 364, 456, 1190, 474, 1196, 456, 1190, 454, 8132, 1280, 364, 1280, 366, 482, 1188, 1280, 366, 1296, 372, 456, 1188, 456, 1214, 1280, 364, 456, 1188, 448, 1220, 456, 1188, 456, 8126, 1280, 366, 1280, 366, 448, 1222, 1280, 366, 1248, 422, 456, 1188, 456, 1190, 1300, 370, 454, 1190, 452, 1218, 454, 1190, 456}; // SYMPHONY D90

void sendPowerCommand()
{
  irsend.sendRaw(rawDataONOFF, 167, rawIrFrequency);
}

void sendPlusCommand()
{
  irsend.sendRaw(rawDataPlus, 191, rawIrFrequency);
}

void sendMinusCommand()
{
  irsend.sendRaw(rawDataMinus, 143, rawIrFrequency);
}

void sendTimerCommand()
{
  irsend.sendRaw(rawDataUhr, 167, rawIrFrequency);
}

int fallbackSpeedPercentFromRpm(int rpm)
{
  if (rpm <= rpmOffThreshold)
  {
    return 0;
  }

  long percent = 1 + ((long)(rpm - rpmPercentOne) * 98 + ((rpmPercentNinetyNine - rpmPercentOne) / 2)) / (rpmPercentNinetyNine - rpmPercentOne);
  return constrain((int)percent, 1, 99);
}

void updateInferredPowerState()
{
  if (RPM >= rpmPowerOnThreshold)
  {
    inferredPowerOn = true;
  }
  else if (RPM <= rpmPowerOffThreshold)
  {
    inferredPowerOn = false;
  }
}

bool payloadIsNumber(const char *payload)
{
  if (!payload || payload[0] == '\0')
  {
    return false;
  }

  for (size_t i = 0; payload[i] != '\0'; i++)
  {
    if (payload[i] < '0' || payload[i] > '9')
    {
      return false;
    }
  }

  return true;
}

const char *default_mqtt_server = "10.17.50.36";
const uint16_t default_mqtt_port = 1883;
const uint32_t mqtt_config_magic = 0x564d5154; // VMQT
const uint16_t mqtt_config_version = 3;
const uint32_t calibration_magic = 0x5643414c; // VCAL
const uint16_t calibration_version = 3;
const size_t eeprom_size = 768;

struct MqttConfigV2
{
  uint32_t magic;
  uint16_t version;
  char server[64];
  uint16_t port;
  char user[32];
  char pass[64];
  uint32_t checksum;
};

struct MqttConfig
{
  uint32_t magic;
  uint16_t version;
  char deviceName[32];
  char server[64];
  uint16_t port;
  char user[32];
  char pass[64];
  uint32_t checksum;
};

struct CalibrationData
{
  uint32_t magic;
  uint16_t version;
  uint8_t count;
  uint16_t rpm[maxCalibrationPoints];
  uint32_t checksum;
};

enum CalibrationState
{
  CAL_IDLE,
  CAL_POWER_ON,
  CAL_MIN_DOWN,
  CAL_MIN_SETTLE,
  CAL_SAMPLE_MIN,
  CAL_PLUS,
  CAL_PLUS_SETTLE,
  CAL_SAMPLE_STEP,
  CAL_DONE,
  CAL_FAILED
};

MqttConfig mqttConfig;
CalibrationData calibrationData;
CalibrationData calibrationWorkingData;
CalibrationState calibrationState = CAL_IDLE;
char mqttCommandTopic[64];
char mqttFanspeedTopic[80];
char mqttStatusTopic[80];
char mqttSpeedPercentTopic[80];
char mqttSpeedSetTopic[84];
char mqttSpeedTargetStateTopic[90];
char mqttPowerStateTopic[80];
char mqttIpAddressTopic[80];
char mqttCalibrationTopic[90];
char mqttCalibrationStatusTopic[98];
String lastCalibrationStatus = "idle";

void saveMqttConfig();
bool calibrationDataIsValid();

WiFiClient espClient;
PubSubClient client(espClient);

int speedPercentFromRpm(int rpm)
{
  if (rpm <= rpmOffThreshold)
  {
    return 0;
  }

  if (!calibrationDataIsValid() || calibrationData.count < 2)
  {
    return fallbackSpeedPercentFromRpm(rpm);
  }

  if (rpm <= calibrationData.rpm[0])
  {
    return 1;
  }

  uint8_t lastIndex = calibrationData.count - 1;
  if (rpm >= calibrationData.rpm[lastIndex])
  {
    return 99;
  }

  for (uint8_t i = 1; i < calibrationData.count; i++)
  {
    uint16_t lowRpm = calibrationData.rpm[i - 1];
    uint16_t highRpm = calibrationData.rpm[i];
    if (rpm <= highRpm && highRpm > lowRpm)
    {
      long lowPercent = 1 + ((long)(i - 1) * 98) / lastIndex;
      long highPercent = 1 + ((long)i * 98) / lastIndex;
      long percent = lowPercent + ((long)(rpm - lowRpm) * (highPercent - lowPercent) + ((highRpm - lowRpm) / 2)) / (highRpm - lowRpm);
      return constrain((int)percent, 1, 99);
    }
  }

  return fallbackSpeedPercentFromRpm(rpm);
}

int calibrationIndexForRpm(int rpm)
{
  if (!calibrationDataIsValid() || calibrationData.count < 2)
  {
    return -1;
  }

  int bestIndex = 0;
  int bestDistance = abs(rpm - (int)calibrationData.rpm[0]);
  for (uint8_t i = 1; i < calibrationData.count; i++)
  {
    int distance = abs(rpm - (int)calibrationData.rpm[i]);
    if (distance < bestDistance)
    {
      bestDistance = distance;
      bestIndex = i;
    }
  }

  return bestIndex;
}

int calibrationIndexForPercent(int percent)
{
  if (!calibrationDataIsValid() || calibrationData.count < 2)
  {
    return -1;
  }

  percent = constrain(percent, 1, 99);
  int lastIndex = calibrationData.count - 1;
  return constrain((int)(((long)(percent - 1) * lastIndex + 49) / 98), 0, lastIndex);
}

int calibratedStepDeltaToTarget(int targetPercent, int currentRpm)
{
  int currentIndex = calibrationIndexForRpm(currentRpm);
  int targetIndex = calibrationIndexForPercent(targetPercent);
  if (currentIndex < 0 || targetIndex < 0)
  {
    return 0;
  }

  return targetIndex - currentIndex;
}

void queueSpeedBurst(int direction, int steps)
{
  speedBurstDirection = direction > 0 ? 1 : -1;
  speedBurstRemaining = constrain(steps, 1, maxSpeedBurstSteps);
  lastSpeedBurstStep = 0;
}

bool processSpeedBurst()
{
  if (speedBurstRemaining <= 0)
  {
    return false;
  }

  if (lastSpeedBurstStep != 0 && (millis() - lastSpeedBurstStep) < speedBurstGap)
  {
    return true;
  }

  if (speedBurstDirection > 0)
  {
    sendPlusCommand();
  }
  else
  {
    sendMinusCommand();
  }

  speedBurstRemaining--;
  lastSpeedBurstStep = millis();
  if (speedBurstRemaining <= 0)
  {
    speedBurstDirection = 0;
    lastSpeedAdjustAttempt = millis();
  }

  return true;
}

void copyStringToBuffer(char *dest, size_t destSize, const String &source)
{
  if (destSize == 0)
  {
    return;
  }
  source.toCharArray(dest, destSize);
  dest[destSize - 1] = '\0';
}

void setDefaultMqttConfig()
{
  memset(&mqttConfig, 0, sizeof(mqttConfig));
  mqttConfig.magic = mqtt_config_magic;
  mqttConfig.version = mqtt_config_version;
  copyStringToBuffer(mqttConfig.deviceName, sizeof(mqttConfig.deviceName), default_device_name);
  copyStringToBuffer(mqttConfig.server, sizeof(mqttConfig.server), default_mqtt_server);
  mqttConfig.port = default_mqtt_port;
}

void terminateMqttConfigStrings()
{
  mqttConfig.deviceName[sizeof(mqttConfig.deviceName) - 1] = '\0';
  mqttConfig.server[sizeof(mqttConfig.server) - 1] = '\0';
  mqttConfig.user[sizeof(mqttConfig.user) - 1] = '\0';
  mqttConfig.pass[sizeof(mqttConfig.pass) - 1] = '\0';
}

uint32_t calculateChecksum(const uint8_t *bytes, size_t length)
{
  uint32_t checksum = 2166136261UL;

  for (size_t i = 0; i < length; i++)
  {
    checksum ^= bytes[i];
    checksum *= 16777619UL;
  }

  return checksum;
}

uint32_t calculateMqttConfigChecksum()
{
  return calculateChecksum((const uint8_t *)&mqttConfig, offsetof(MqttConfig, checksum));
}

uint32_t calculateMqttConfigV2Checksum(const MqttConfigV2 &config)
{
  return calculateChecksum((const uint8_t *)&config, offsetof(MqttConfigV2, checksum));
}

uint32_t calculateCalibrationChecksum()
{
  return calculateChecksum((const uint8_t *)&calibrationData, offsetof(CalibrationData, checksum));
}

uint32_t calculateCalibrationChecksum(const CalibrationData &data)
{
  return calculateChecksum((const uint8_t *)&data, offsetof(CalibrationData, checksum));
}

size_t calibrationEepromOffset()
{
  return sizeof(MqttConfig);
}

bool calibrationDataIsValid(const CalibrationData &data)
{
  return data.magic == calibration_magic &&
         data.version == calibration_version &&
         data.count <= maxCalibrationPoints &&
         data.checksum == calculateCalibrationChecksum(data);
}

bool calibrationDataIsValid()
{
  return calibrationDataIsValid(calibrationData);
}

void clearCalibrationData(CalibrationData &data)
{
  memset(&data, 0, sizeof(data));
  data.magic = calibration_magic;
  data.version = calibration_version;
  data.checksum = calculateCalibrationChecksum(data);
}

void clearCalibrationData()
{
  clearCalibrationData(calibrationData);
}

void saveCalibrationData()
{
  calibrationData.magic = calibration_magic;
  calibrationData.version = calibration_version;
  calibrationData.checksum = calculateCalibrationChecksum();
  EEPROM.put(calibrationEepromOffset(), calibrationData);
  EEPROM.commit();
}

void loadCalibrationData()
{
  EEPROM.get(calibrationEepromOffset(), calibrationData);
  if (!calibrationDataIsValid())
  {
    clearCalibrationData();
  }
  clearCalibrationData(calibrationWorkingData);
}

bool mqttConfigIsValid()
{
  terminateMqttConfigStrings();
  return mqttConfig.magic == mqtt_config_magic &&
         mqttConfig.version == mqtt_config_version &&
         mqttConfig.port > 0 &&
         strlen(mqttConfig.deviceName) > 0 &&
         strlen(mqttConfig.server) > 0 &&
         mqttConfig.checksum == calculateMqttConfigChecksum();
}

bool migrateMqttConfigV2()
{
  MqttConfigV2 oldConfig;
  EEPROM.get(0, oldConfig);
  oldConfig.server[sizeof(oldConfig.server) - 1] = '\0';
  oldConfig.user[sizeof(oldConfig.user) - 1] = '\0';
  oldConfig.pass[sizeof(oldConfig.pass) - 1] = '\0';

  if (oldConfig.magic != mqtt_config_magic ||
      oldConfig.version != 2 ||
      oldConfig.port == 0 ||
      strlen(oldConfig.server) == 0 ||
      oldConfig.checksum != calculateMqttConfigV2Checksum(oldConfig))
  {
    return false;
  }

  setDefaultMqttConfig();
  copyStringToBuffer(mqttConfig.server, sizeof(mqttConfig.server), oldConfig.server);
  mqttConfig.port = oldConfig.port;
  copyStringToBuffer(mqttConfig.user, sizeof(mqttConfig.user), oldConfig.user);
  copyStringToBuffer(mqttConfig.pass, sizeof(mqttConfig.pass), oldConfig.pass);
  saveMqttConfig();
  return true;
}

void loadMqttConfig()
{
  EEPROM.begin(eeprom_size);
  EEPROM.get(0, mqttConfig);

  if (!mqttConfigIsValid() && !migrateMqttConfigV2())
  {
    setDefaultMqttConfig();
    mqttConfig.checksum = calculateMqttConfigChecksum();
    EEPROM.put(0, mqttConfig);
    EEPROM.commit();
  }

  loadCalibrationData();
}

void saveMqttConfig()
{
  mqttConfig.magic = mqtt_config_magic;
  mqttConfig.version = mqtt_config_version;
  terminateMqttConfigStrings();
  mqttConfig.checksum = calculateMqttConfigChecksum();
  EEPROM.put(0, mqttConfig);
  EEPROM.commit();
}

String htmlEscape(const String &input)
{
  String output;
  output.reserve(input.length());
  for (size_t i = 0; i < input.length(); i++)
  {
    char c = input.charAt(i);
    if (c == '&')
    {
      output += F("&amp;");
    }
    else if (c == '<')
    {
      output += F("&lt;");
    }
    else if (c == '>')
    {
      output += F("&gt;");
    }
    else if (c == '"')
    {
      output += F("&quot;");
    }
    else
    {
      output += c;
    }
  }
  return output;
}

void applyMqttConfig()
{
  client.setServer(mqttConfig.server, mqttConfig.port);
}

void updateMqttTopics()
{
  snprintf(mqttCommandTopic, sizeof(mqttCommandTopic), "Vornado/%s", mqttConfig.deviceName);
  snprintf(mqttFanspeedTopic, sizeof(mqttFanspeedTopic), "Vornado/%s/fanspeed", mqttConfig.deviceName);
  snprintf(mqttStatusTopic, sizeof(mqttStatusTopic), "Vornado/%s/status", mqttConfig.deviceName);
  snprintf(mqttSpeedPercentTopic, sizeof(mqttSpeedPercentTopic), "Vornado/%s/speed", mqttConfig.deviceName);
  snprintf(mqttSpeedSetTopic, sizeof(mqttSpeedSetTopic), "Vornado/%s/speed/set", mqttConfig.deviceName);
  snprintf(mqttSpeedTargetStateTopic, sizeof(mqttSpeedTargetStateTopic), "Vornado/%s/speed/target", mqttConfig.deviceName);
  snprintf(mqttPowerStateTopic, sizeof(mqttPowerStateTopic), "Vornado/%s/power", mqttConfig.deviceName);
  snprintf(mqttIpAddressTopic, sizeof(mqttIpAddressTopic), "Vornado/%s/ip", mqttConfig.deviceName);
  snprintf(mqttCalibrationTopic, sizeof(mqttCalibrationTopic), "Vornado/%s/calibration/set", mqttConfig.deviceName);
  snprintf(mqttCalibrationStatusTopic, sizeof(mqttCalibrationStatusTopic), "Vornado/%s/calibration/status", mqttConfig.deviceName);
}

String normalizeDeviceName(String name)
{
  name.trim();
  String normalized;
  normalized.reserve(31);

  for (size_t i = 0; i < name.length() && normalized.length() < 31; i++)
  {
    char c = name.charAt(i);
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-')
    {
      normalized += c;
    }
    else if (normalized.length() > 0 && normalized.charAt(normalized.length() - 1) != '-')
    {
      normalized += '-';
    }
  }

  while (normalized.endsWith("-"))
  {
    normalized.remove(normalized.length() - 1);
  }

  if (normalized.length() == 0)
  {
    normalized = default_device_name;
  }

  return normalized;
}

String discoveryId()
{
  String id = "vornado_";
  for (size_t i = 0; i < strlen(mqttConfig.deviceName); i++)
  {
    char c = mqttConfig.deviceName[i];
    if (c >= 'A' && c <= 'Z')
    {
      c = c + ('a' - 'A');
    }
    id += c == '-' ? '_' : c;
  }
  return id;
}

String discoveryDeviceJson()
{
  String id = discoveryId();
  String device = F("\"device\":{\"identifiers\":[\"");
  device += id;
  device += F("\"],\"name\":\"");
  device += mqttConfig.deviceName;
  device += F("\",\"manufacturer\":\"Custom\",\"model\":\"Vornado IR MQTT\"}");
  return device;
}

bool publishRetained(const String &topic, const String &payload)
{
  bool ok = client.publish(topic.c_str(), payload.c_str(), true);
  if (!ok)
  {
    Serial.print("MQTT publish failed: ");
    Serial.println(topic);
  }
  return ok;
}

int currentTargetStatePercent()
{
  if (targetSpeedPercent >= 1)
  {
    return targetSpeedPercent;
  }

  return max(1, speedPercentFromRpm(RPM));
}

void publishCalibrationStatus(const String &status)
{
  lastCalibrationStatus = status;
  Serial.print("Calibration status: ");
  Serial.println(status);
  if (client.connected())
  {
    publishRetained(mqttCalibrationStatusTopic, status);
  }
}

void startCalibration()
{
  if (calibrationState != CAL_IDLE)
  {
    publishCalibrationStatus(F("busy"));
    return;
  }

  targetSpeedPercent = -1;
  powerToggleAttempts = 0;
  speedBurstRemaining = 0;
  speedBurstDirection = 0;
  calibrationActionCount = 0;
  calibrationRetryCount = 0;
  calibrationStableCount = 0;
  calibrationNextAction = 0;
  clearCalibrationData(calibrationWorkingData);
  calibrationState = CAL_POWER_ON;
  publishCalibrationStatus(F("starting"));
}

void failCalibration(const String &reason)
{
  calibrationState = CAL_FAILED;
  publishCalibrationStatus(String(F("failed: ")) + reason);
}

void finishCalibration()
{
  if (calibrationWorkingData.count < 2)
  {
    failCalibration(F("not enough points"));
    return;
  }

  calibrationData = calibrationWorkingData;
  calibrationState = CAL_DONE;
  saveCalibrationData();
  publishCalibrationStatus(String(F("done: ")) + calibrationData.count + F(" points"));
}

void addCalibrationPoint(CalibrationData &data, uint16_t rpm)
{
  if (data.count >= maxCalibrationPoints)
  {
    return;
  }

  data.rpm[data.count] = rpm;
  data.count++;
  data.checksum = calculateCalibrationChecksum(data);
}

void processCalibration()
{
  if (calibrationState == CAL_IDLE)
  {
    return;
  }

  unsigned long now = millis();

  if (calibrationState == CAL_DONE || calibrationState == CAL_FAILED)
  {
    calibrationState = CAL_IDLE;
    return;
  }

  if (now < calibrationNextAction)
  {
    return;
  }

  switch (calibrationState)
  {
  case CAL_POWER_ON:
    if (inferredPowerOn)
    {
      calibrationActionCount = 0;
      calibrationState = CAL_MIN_DOWN;
      publishCalibrationStatus(F("min_down"));
      return;
    }

    if (calibrationRetryCount >= maxPowerToggleAttempts)
    {
      failCalibration(F("power on"));
      return;
    }

    sendPowerCommand();
    calibrationRetryCount++;
    calibrationNextAction = now + powerOnGracePeriod;
    publishCalibrationStatus(F("power_on"));
    return;

  case CAL_MIN_DOWN:
    if (calibrationActionCount < calibrationMinDownSteps)
    {
      sendMinusCommand();
      calibrationActionCount++;
      calibrationNextAction = now + calibrationIrGap;
      return;
    }

    calibrationState = CAL_MIN_SETTLE;
    calibrationNextAction = now + calibrationSettlePeriod;
    publishCalibrationStatus(F("settle_min"));
    return;

  case CAL_MIN_SETTLE:
    calibrationState = CAL_SAMPLE_MIN;
    return;

  case CAL_SAMPLE_MIN:
    if (RPM <= rpmOffThreshold)
    {
      failCalibration(F("no rpm at minimum"));
      return;
    }

    addCalibrationPoint(calibrationWorkingData, (uint16_t)RPM);
    calibrationActionCount = 0;
    calibrationStableCount = 0;
    calibrationState = CAL_PLUS;
    publishCalibrationStatus(String(F("point 1 rpm ")) + RPM);
    return;

  case CAL_PLUS:
    if (calibrationWorkingData.count >= maxCalibrationPoints)
    {
      finishCalibration();
      return;
    }

    sendPlusCommand();
    calibrationActionCount++;
    calibrationState = CAL_PLUS_SETTLE;
    calibrationNextAction = now + calibrationSettlePeriod;
    publishCalibrationStatus(String(F("plus ")) + calibrationActionCount);
    return;

  case CAL_PLUS_SETTLE:
    calibrationState = CAL_SAMPLE_STEP;
    return;

  case CAL_SAMPLE_STEP:
  {
    uint16_t previousRpm = calibrationWorkingData.count > 0 ? calibrationWorkingData.rpm[calibrationWorkingData.count - 1] : 0;
    addCalibrationPoint(calibrationWorkingData, (uint16_t)RPM);

    if (RPM > previousRpm + calibrationRpmIncreaseThreshold)
    {
      calibrationStableCount = 0;
      publishCalibrationStatus(String(F("point ")) + calibrationWorkingData.count + F(" rpm ") + RPM);
    }
    else
    {
      calibrationStableCount++;
      publishCalibrationStatus(String(F("point ")) + calibrationWorkingData.count + F(" rpm ") + RPM + F(" stable ") + calibrationStableCount);
    }

    if (calibrationWorkingData.count >= maxCalibrationPoints)
    {
      finishCalibration();
      return;
    }

    calibrationState = CAL_PLUS;
    return;
  }

  default:
    return;
  }
}

void publishButtonDiscovery(const char *object, const char *name, const char *payload)
{
  String id = discoveryId();
  String topic = F("homeassistant/button/");
  topic += id;
  topic += "_";
  topic += object;
  topic += F("/config");

  String config = F("{\"name\":\"");
  config += name;
  config += F("\",\"unique_id\":\"");
  config += id;
  config += "_";
  config += object;
  config += F("\",\"command_topic\":\"");
  config += mqttCommandTopic;
  config += F("\",\"payload_press\":\"");
  config += payload;
  config += F("\",");
  config += discoveryDeviceJson();
  config += F("}");

  publishRetained(topic, config);
}

void publishSensorDiscovery()
{
  String id = discoveryId();
  String topic = F("homeassistant/sensor/");
  topic += id;
  topic += F("_fanspeed/config");

  String config = F("{\"name\":\"Drehzahl\",\"unique_id\":\"");
  config += id;
  config += F("_fanspeed\",\"state_topic\":\"");
  config += mqttFanspeedTopic;
  config += F("\",\"unit_of_measurement\":\"RPM\",\"icon\":\"mdi:fan\",");
  config += discoveryDeviceJson();
  config += F("}");

  publishRetained(topic, config);
}

void publishSpeedPercentDiscovery()
{
  String id = discoveryId();
  String topic = F("homeassistant/sensor/");
  topic += id;
  topic += F("_speed_percent/config");

  String config = F("{\"name\":\"Luefterstufe\",\"unique_id\":\"");
  config += id;
  config += F("_speed_percent\",\"state_topic\":\"");
  config += mqttSpeedPercentTopic;
  config += F("\",\"unit_of_measurement\":\"%\",\"icon\":\"mdi:fan-speed-3\",");
  config += discoveryDeviceJson();
  config += F("}");

  publishRetained(topic, config);
}

void publishPowerStateDiscovery()
{
  String id = discoveryId();
  String topic = F("homeassistant/binary_sensor/");
  topic += id;
  topic += F("_power/config");

  String config = F("{\"name\":\"Power Status\",\"unique_id\":\"");
  config += id;
  config += F("_power\",\"state_topic\":\"");
  config += mqttPowerStateTopic;
  config += F("\",\"payload_on\":\"ON\",\"payload_off\":\"OFF\",\"device_class\":\"power\",");
  config += discoveryDeviceJson();
  config += F("}");

  publishRetained(topic, config);
}

void publishIpAddressDiscovery()
{
  String id = discoveryId();
  String topic = F("homeassistant/sensor/");
  topic += id;
  topic += F("_ip_address/config");

  String config = F("{\"name\":\"IP Adresse\",\"unique_id\":\"");
  config += id;
  config += F("_ip_address\",\"state_topic\":\"");
  config += mqttIpAddressTopic;
  config += F("\",\"icon\":\"mdi:ip-network\",");
  config += discoveryDeviceJson();
  config += F("}");

  publishRetained(topic, config);
}

void publishCalibrationButtonDiscovery()
{
  String id = discoveryId();
  String topic = F("homeassistant/button/");
  topic += id;
  topic += F("_calibrate/config");

  String config = F("{\"name\":\"Kalibrierung starten\",\"unique_id\":\"");
  config += id;
  config += F("_calibrate\",\"command_topic\":\"");
  config += mqttCalibrationTopic;
  config += F("\",\"payload_press\":\"start\",\"icon\":\"mdi:tune-variant\",");
  config += discoveryDeviceJson();
  config += F("}");

  publishRetained(topic, config);
}

void publishCalibrationStatusDiscovery()
{
  String id = discoveryId();
  String topic = F("homeassistant/sensor/");
  topic += id;
  topic += F("_calibration_status/config");

  String config = F("{\"name\":\"Kalibrierung Status\",\"unique_id\":\"");
  config += id;
  config += F("_calibration_status\",\"state_topic\":\"");
  config += mqttCalibrationStatusTopic;
  config += F("\",\"icon\":\"mdi:progress-wrench\",");
  config += discoveryDeviceJson();
  config += F("}");

  publishRetained(topic, config);
}

void publishSpeedSliderDiscovery()
{
  String id = discoveryId();
  String topic = F("homeassistant/number/");
  topic += id;
  topic += F("_speed_set/config");

  String config = F("{\"name\":\"Luefterstufe Soll\",\"unique_id\":\"");
  config += id;
  config += F("_speed_set\",\"command_topic\":\"");
  config += mqttSpeedSetTopic;
  config += F("\",\"state_topic\":\"");
  config += mqttSpeedTargetStateTopic;
  config += F("\",\"min\":1,\"max\":99,\"step\":1,\"mode\":\"slider\",\"unit_of_measurement\":\"%\",\"icon\":\"mdi:fan-speed-3\",");
  config += discoveryDeviceJson();
  config += F("}");

  publishRetained(topic, config);
}

void publishHomeAssistantDiscovery()
{
  if (!client.connected())
  {
    return;
  }

  publishButtonDiscovery("power", "Power", "power");
  publishButtonDiscovery("plus", "Plus", "plus");
  publishButtonDiscovery("minus", "Minus", "minus");
  publishButtonDiscovery("timer", "Timer", "uhr");
  publishSensorDiscovery();
  publishSpeedPercentDiscovery();
  publishPowerStateDiscovery();
  publishIpAddressDiscovery();
  publishCalibrationButtonDiscovery();
  publishCalibrationStatusDiscovery();
  publishSpeedSliderDiscovery();
}

bool requireWebAuth()
{
  if (httpServer.authenticate(web_user, web_password))
  {
    return true;
  }

  httpServer.requestAuthentication();
  return false;
}

void handleConfigPage()
{
  if (!requireWebAuth())
  {
    return;
  }

  String page;
  page.reserve(3200);
  page += F("<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>");
  page += F("<title>Vornado MQTT</title><style>body{font-family:-apple-system,BlinkMacSystemFont,Segoe UI,sans-serif;margin:24px;max-width:520px}label{display:block;margin:14px 0 4px}input{box-sizing:border-box;width:100%;padding:10px;font-size:16px}button{margin-top:18px;padding:10px 14px;font-size:16px}a{display:inline-block;margin-top:18px}.row{margin-top:10px}.hint{color:#666;font-size:14px}</style></head><body>");
  page += F("<h1>Vornado MQTT</h1><form method='post' action='/config'>");
  page += F("<p class='hint'>Firmware Version: ");
  page += firmware_version;
  page += F("</p>");
  page += F("<label for='device'>Geraetename</label><input id='device' name='device' value='");
  page += htmlEscape(String(mqttConfig.deviceName));
  page += F("' required maxlength='31'>");
  page += F("<label for='server'>Broker IP / Host</label><input id='server' name='server' value='");
  page += htmlEscape(String(mqttConfig.server));
  page += F("' required maxlength='63'>");
  page += F("<label for='port'>Port</label><input id='port' name='port' type='number' min='1' max='65535' value='");
  page += String(mqttConfig.port);
  page += F("' required>");
  page += F("<label for='user'>MQTT Nutzer</label><input id='user' name='user' value='");
  page += htmlEscape(String(mqttConfig.user));
  page += F("' maxlength='31'>");
  page += F("<label for='pass'>MQTT Passwort</label><input id='pass' name='pass' type='password' maxlength='63' placeholder='leer lassen = behalten'>");
  page += F("<div class='row'><label><input style='width:auto' type='checkbox' name='clear_pass' value='1'> Passwort loeschen</label></div>");
  page += F("<button type='submit'>Speichern</button></form>");
  page += F("<p class='hint'>Aktueller MQTT Status: ");
  page += client.connected() ? F("verbunden") : F("nicht verbunden");
  page += F("<br>Command Topic: ");
  page += htmlEscape(String(mqttCommandTopic));
  page += F("<br>Fanspeed Topic: ");
  page += htmlEscape(String(mqttFanspeedTopic));
  page += F("<br>Status Topic: ");
  page += htmlEscape(String(mqttStatusTopic));
  page += F("<br>Speed Topic: ");
  page += htmlEscape(String(mqttSpeedPercentTopic));
  page += F("<br>Speed Set Topic: ");
  page += htmlEscape(String(mqttSpeedSetTopic));
  page += F("<br>Speed Target State Topic: ");
  page += htmlEscape(String(mqttSpeedTargetStateTopic));
  page += F("<br>Power State Topic: ");
  page += htmlEscape(String(mqttPowerStateTopic));
  page += F("<br>IP Address Topic: ");
  page += htmlEscape(String(mqttIpAddressTopic));
  page += F("<br>Calibration Topic: ");
  page += htmlEscape(String(mqttCalibrationTopic));
  page += F("<br>Calibration Status Topic: ");
  page += htmlEscape(String(mqttCalibrationStatusTopic));
  page += F("<br>Kalibrierpunkte: ");
  page += String(calibrationData.count);
  page += F("<br>Kalibrierstatus: ");
  page += htmlEscape(lastCalibrationStatus);
  page += F("</p><form method='post' action='/restart'><button type='submit'>Neustart</button></form>");
  page += F("<a href='/update'>Firmware update</a></body></html>");
  httpServer.send(200, F("text/html"), page);
}

void handleConfigSave()
{
  if (!requireWebAuth())
  {
    return;
  }

  if (httpServer.hasArg("device"))
  {
    copyStringToBuffer(mqttConfig.deviceName, sizeof(mqttConfig.deviceName), normalizeDeviceName(httpServer.arg("device")));
  }

  if (httpServer.hasArg("server"))
  {
    copyStringToBuffer(mqttConfig.server, sizeof(mqttConfig.server), httpServer.arg("server"));
  }

  if (strlen(mqttConfig.server) == 0)
  {
    copyStringToBuffer(mqttConfig.server, sizeof(mqttConfig.server), default_mqtt_server);
  }

  if (httpServer.hasArg("port"))
  {
    long port = httpServer.arg("port").toInt();
    mqttConfig.port = port >= 1 && port <= 65535 ? (uint16_t)port : default_mqtt_port;
  }

  if (httpServer.hasArg("user"))
  {
    copyStringToBuffer(mqttConfig.user, sizeof(mqttConfig.user), httpServer.arg("user"));
  }

  if (httpServer.hasArg("clear_pass"))
  {
    mqttConfig.pass[0] = '\0';
  }
  else if (httpServer.hasArg("pass") && httpServer.arg("pass").length() > 0)
  {
    copyStringToBuffer(mqttConfig.pass, sizeof(mqttConfig.pass), httpServer.arg("pass"));
  }

  saveMqttConfig();
  updateMqttTopics();
  applyMqttConfig();
  client.disconnect();
  lastMqttReconnectAttempt = 0;

  httpServer.sendHeader(F("Location"), F("/config"));
  httpServer.send(303);
}

void handleRestart()
{
  if (!requireWebAuth())
  {
    return;
  }

  httpServer.send(200, F("text/html"), F("<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'><title>Neustart</title></head><body><h1>Neustart</h1><p>Der Vornado Controller startet neu.</p></body></html>"));
  delay(500);
  ESP.restart();
}

void setup_wifi()
{
  delay(100);

  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.hostname(mqttConfig.deviceName);
  WiFi.begin(ssid, password);
  unsigned long wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - wifiStart) < wifiConnectTimeout)
  {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("");
    Serial.println("WiFi connection failed. Starting setup access point.");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(setup_ap_ssid, setup_ap_password);
    Serial.print("Setup AP IP address: ");
    Serial.println(WiFi.softAPIP());
    return;
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

  char payload_str[32];
  if (length >= sizeof(payload_str))
  {
    return;
  }

  memcpy(payload_str, payload, length);
  payload_str[length] = '\0';

  if (String(topic) == mqttCalibrationTopic)
  {
    if (String(payload_str) == "start" || String(payload_str) == "calibrate")
    {
      startCalibration();
    }
    return;
  }

  if (String(topic) == mqttSpeedSetTopic)
  {
    if (calibrationState != CAL_IDLE)
    {
      Serial.print(" ignored target speed while calibration is running");
      return;
    }

    if (!payloadIsNumber(payload_str))
    {
      Serial.print(" ignored non-numeric target speed:");
      Serial.print(payload_str);
      return;
    }

    int requestedSpeed = String(payload_str).toInt();
    if (requestedSpeed < 1 || requestedSpeed > 99)
    {
      Serial.print(" ignored out-of-range target speed:");
      Serial.print(payload_str);
      return;
    }

    targetSpeedPercent = requestedSpeed;
    powerToggleAttempts = 0;
    speedBurstRemaining = 0;
    speedBurstDirection = 0;
    lastSpeedAdjustAttempt = 0;
    powerOnGraceUntil = 0;
    powerOffWaitUntil = 0;
    Serial.print(" target speed is:");
    Serial.print(targetSpeedPercent);
    if (client.connected())
    {
      client.publish(mqttSpeedTargetStateTopic, String(targetSpeedPercent).c_str(), true);
    }
    return;
  }

  if (String(topic) == mqttCommandTopic)
  {
    if (calibrationState != CAL_IDLE)
    {
      publishCalibrationStatus(F("busy"));
      return;
    }

    Serial.print((String)payload_str);

    // Luefter AN/AUS
    if (String(payload_str) == "power")
    {
      targetSpeedPercent = -1;
      powerToggleAttempts = 0;
      speedBurstRemaining = 0;
      speedBurstDirection = 0;
      sendPowerCommand();
    }
    // Luefterstufe plus
    if (String(payload_str) == "plus")
    {
      targetSpeedPercent = -1;
      powerToggleAttempts = 0;
      speedBurstRemaining = 0;
      speedBurstDirection = 0;
      sendPlusCommand();
    }
    // Luefterstufe minus
    if (String(payload_str) == "minus")
    {
      targetSpeedPercent = -1;
      powerToggleAttempts = 0;
      speedBurstRemaining = 0;
      speedBurstDirection = 0;
      sendMinusCommand();
    }
    // Lueftertimer
    if (String(payload_str) == "uhr")
    {
      targetSpeedPercent = -1;
      powerToggleAttempts = 0;
      speedBurstRemaining = 0;
      speedBurstDirection = 0;
      sendTimerCommand();
    }
  }
}

void reconnect()
{
  if (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");

    String clientId = mqttConfig.deviceName;
    clientId += "-";
    clientId += String(random(0xffff), HEX);

    bool connected;
    if (strlen(mqttConfig.user) > 0)
    {
      connected = client.connect(clientId.c_str(), mqttConfig.user, mqttConfig.pass, mqttStatusTopic, 1, true, "offline");
    }
    else
    {
      connected = client.connect(clientId.c_str(), mqttStatusTopic, 1, true, "offline");
    }

    if (connected)
    {
      Serial.println("connected");

      client.publish(mqttStatusTopic, "online", true);
      client.subscribe(mqttCommandTopic);
      client.subscribe(mqttSpeedSetTopic);
      client.subscribe(mqttCalibrationTopic);
      publishHomeAssistantDiscovery();
      publishRetained(mqttIpAddressTopic, WiFi.localIP().toString());
      publishRetained(mqttCalibrationStatusTopic, lastCalibrationStatus);
      publishRetained(mqttSpeedTargetStateTopic, String(currentTargetStatePercent()));
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again later");
    }
  }
}

void setup()
{
  irsend.begin();
  Serial.begin(9600);

  loadMqttConfig();
  updateMqttTopics();
  setup_wifi();
  applyMqttConfig();
  client.setBufferSize(768);
  client.setCallback(callback);

  previousmills = 0;
  lastMqttReconnectAttempt = 0;
  lastSpeedAdjustAttempt = 0;
  lastSpeedBurstStep = 0;
  powerOnGraceUntil = 0;
  powerOffWaitUntil = 0;
  lastTelemetryPublish = 0;
  powerToggleAttempts = 0;
  speedBurstRemaining = 0;
  speedBurstDirection = 0;
  lastPublishedRpm = -1;
  lastPublishedSpeedPercent = -1;
  lastPublishedPowerState = -1;
  inferredPowerOn = false;
  interruptCounter = 0;
  RPM = 0;

  // NODEMCU esp8266

  pinMode(tachInputPIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(tachInputPIN), handleInterrupt, FALLING);

  MDNS.begin(mqttConfig.deviceName);

  httpServer.on("/", HTTP_GET, handleConfigPage);
  httpServer.on("/config", HTTP_GET, handleConfigPage);
  httpServer.on("/config", HTTP_POST, handleConfigSave);
  httpServer.on("/restart", HTTP_POST, handleRestart);
  httpUpdater.setup(&httpServer, "/update", web_user, web_password);
  httpServer.begin();

  MDNS.addService("http", "tcp", 80);
  Serial.printf("HTTPUpdateServer ready! Open http://%s.local/update in your browser\n", mqttConfig.deviceName);
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
  updateInferredPowerState();

  if (client.connected())
  {
    int speedPercent = speedPercentFromRpm(RPM);
    int powerState = inferredPowerOn ? 1 : 0;
    bool periodicPublish = (millis() - lastTelemetryPublish) >= telemetryPublishPeriod;

    if (periodicPublish || RPM != lastPublishedRpm)
    {
      client.publish(mqttFanspeedTopic, String(RPM).c_str(), true);
      lastPublishedRpm = RPM;
    }

    if (periodicPublish || speedPercent != lastPublishedSpeedPercent)
    {
      client.publish(mqttSpeedPercentTopic, String(speedPercent).c_str(), true);
      lastPublishedSpeedPercent = speedPercent;
    }

    if (periodicPublish || powerState != lastPublishedPowerState)
    {
      client.publish(mqttPowerStateTopic, inferredPowerOn ? "ON" : "OFF", true);
      lastPublishedPowerState = powerState;
    }

    if (periodicPublish)
    {
      lastTelemetryPublish = millis();
    }
  }
}

void adjustSpeedToTarget()
{
  if (processSpeedBurst())
  {
    return;
  }

  if (targetSpeedPercent < 0 || (millis() - lastSpeedAdjustAttempt) < speedAdjustPeriod)
  {
    return;
  }

  int currentSpeedPercent = speedPercentFromRpm(RPM);

  if (!inferredPowerOn)
  {
    if (powerToggleAttempts >= maxPowerToggleAttempts)
    {
      Serial.println("Speed target aborted: power on retry limit reached");
      targetSpeedPercent = -1;
      powerToggleAttempts = 0;
      return;
    }

    if (millis() < powerOnGraceUntil)
    {
      return;
    }

    sendPowerCommand();
    powerToggleAttempts++;
    lastSpeedAdjustAttempt = millis();
    powerOnGraceUntil = millis() + powerOnGracePeriod;
    return;
  }

  if (millis() < powerOnGraceUntil)
  {
    return;
  }

  powerToggleAttempts = 0;

  int delta = targetSpeedPercent - currentSpeedPercent;
  if (abs(delta) <= 1)
  {
    targetSpeedPercent = -1;
    return;
  }

  if (calibrationDataIsValid() && calibrationData.count >= 2)
  {
    int stepDelta = calibratedStepDeltaToTarget(targetSpeedPercent, RPM);
    if (stepDelta != 0)
    {
      int steps = constrain(abs(stepDelta), 1, maxSpeedBurstSteps);
      queueSpeedBurst(stepDelta > 0 ? 1 : -1, steps);
      return;
    }
  }

  if (delta > 0)
  {
    sendPlusCommand();
  }
  else
  {
    sendMinusCommand();
  }

  lastSpeedAdjustAttempt = millis();
}

void loop()
{

  httpServer.handleClient();
  MDNS.update();

  if (WiFi.status() == WL_CONNECTED && !client.connected() && (millis() - lastMqttReconnectAttempt > 5000))
  {
    lastMqttReconnectAttempt = millis();
    reconnect();
  }

  if (client.connected())
  {
    client.loop();
  }

  processCalibration();

  if (client.connected() && calibrationState == CAL_IDLE)
  {
    adjustSpeedToTarget();
  }

  if ((millis() - previousmills) > calculationPeriod)
  { // Process counters once every second
    previousmills = millis();
    noInterrupts();
    int count = interruptCounter;
    interruptCounter = 0;
    interrupts();
    computeFanSpeed(count);
    displayFanSpeed();
  }
  yield();
}
