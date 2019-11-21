/*
  +----------------------------------------------------------------------+
  | Coogle WiFi 4x4 Keypad                                               |
  +----------------------------------------------------------------------+
  | Copyright (c) 2017-2019 John Coggeshall                              |
  +----------------------------------------------------------------------+
  | Licensed under the Apache License, Version 2.0 (the "License");      |
  | you may not use this file except in compliance with the License. You |
  | may obtain a copy of the License at:                                 |
  |                                                                      |
  | http://www.apache.org/licenses/LICENSE-2.0                           |
  |                                                                      |
  | Unless required by applicable law or agreed to in writing, software  |
  | distributed under the License is distributed on an "AS IS" BASIS,    |
  | WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or      |
  | implied. See the License for the specific language governing         |
  | permissions and limitations under the License.                       |
  +----------------------------------------------------------------------+
  | Authors: John Coggeshall <john@thissmarthouse.com>                   |
  +----------------------------------------------------------------------+
*/
#ifndef __APP_H_
#define __APP_H_

#include "buildinfo.h"
#include "rboot.h"
#include "ArduinoJson.h"
#include <rboot-api.h>
#include "CoogleIOT_Logger.h"
#include "CoogleIOT_Wifi.h"
#include "CoogleIOT_NTP.h"
#include "CoogleIOT_MQTT.h"
#include "CoogleIOT_OTA.h"
#include "CoogleIOT_Config.h"
#include "logger.h"
#include <PubSubClient.h>

#ifndef SERIAL_BAUD
#define SERIAL_BAUD 115200
#endif

#ifndef DEBUG_HOSTNAME
#define DEBUG_HOSTNAME "coogle-switch"
#endif

#ifndef APP_NAME
#define APP_NAME "Coogle SWB1 4-Relay Switch"
#endif

#ifndef MQTT_TOPIC_MAX_LEN
#define MQTT_TOPIC_MAX_LEN 64
#endif

#ifndef NUM_SWITCHES
#define NUM_SWITCHES 1
#endif

#ifndef NUM_SWITCHES
#error "Must have NUM_SWITCHES defined"
#endif

#if NUM_SWITCHES < 1
#error "Invalid valid for NUM_SWITCHES"
#endif 

#if NUM_SWITCHES > 3
#error "Invalid configuration, number of switches can't be greater than 3"
#endif

#define LED_BUILTIN 13

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

typedef struct app_config_t {
	coogleiot_config_base_t base;
	char mqtt_topic[MQTT_TOPIC_MAX_LEN + 1] = {NULL};
  char mqtt_state_topic[MQTT_TOPIC_MAX_LEN + 1] = {NULL};
  bool turn_on_at_boot = false;
  uint16_t debounce_time = 200;
};

app_config_t *app_config = NULL;

static uint8_t switch_map[NUM_SWITCHES] = {
#if NUM_SWITCHES == 1
  12, 
#endif
#if NUM_SWITCHES == 2
  5, 
#endif
#if NUM_SWITCHES == 3
  4, 
#endif
};

static uint8_t button_map[NUM_SWITCHES] = {
#if NUM_SWITCHES == 1
  0,
#endif
#if NUM_SWITCHES == 2
  9,
#endif
#if NUM_SWITCHES == 3
  10,
#endif
};

static int buttonReadings[NUM_SWITCHES] = {LOW};
long lastToggleTimes[NUM_SWITCHES] = {0};
int currentButtonReading = LOW;

class CoogleIOT_Logger;
class CoogleIOT_Wifi;
class CoogleIOT_NTP;
class CoogleIOT_MQTT;
class CoogleIOT_OTA;
class CoogleIOT_Config;

CoogleIOT_Config *configManager = NULL;
CoogleIOT_Logger *_ciot_log = NULL;
CoogleIOT_Wifi *WiFiManager = NULL;
CoogleIOT_NTP *NTPManager = NULL;
CoogleIOT_MQTT *mqttManager = NULL;
CoogleIOT_OTA *otaManager = NULL;

PubSubClient *mqtt = NULL;

bool onParseConfig(DynamicJsonDocument&);

void onMQTTConnect();
void onMQTTCommand(const char *, byte *, unsigned int);
void logSetupInfo();

void onNTPReady();
void onNewFirmware();

void setupConfig();
void setupSerial();
void setupMQTT();
void setupNTP();
void setupLogging();
void setupWiFi();
void setupOTA();

void setup();
void loop();

#endif
