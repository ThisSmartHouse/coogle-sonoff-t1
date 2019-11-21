/*
  +----------------------------------------------------------------------+
  | Coogle SWB1 4-Port SmartSwitch                                       |
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

/**
 * Arduino libs must be included in the .ino file to be detected by the build system
 */
#include <FS.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <Hash.h>
#include <PubSubClient.h>
#include "app.h"

bool ota_ready = false;
bool restart = false;

void setupSerial()
{
	if(Serial) {
		return;
	}

	Serial.begin(SERIAL_BAUD);

	for(int i = 0; (i < 500000) && !Serial; i++) {
		yield();
	}


	Serial.printf(APP_NAME " v%s (%s) (built: %s %s)\r\n", _BuildInfo.src_version, _BuildInfo.env_version, _BuildInfo.date, _BuildInfo.time);
}

void setupRelays()
{
	char msg[150];
	app_config_t *config;

	config = (app_config_t *)configManager->getConfig();

    for(int i = 0; i < sizeof(switch_map) / sizeof(uint8_t); i++) {
     	
     	pinMode(switch_map[i], OUTPUT);
     	
     	if(config->turn_on_at_boot) {
     		digitalWrite(switch_map[i], HIGH);
     	} else {
     		digitalWrite(switch_map[i], LOW);
     	}

     	snprintf(msg, 150, config->mqtt_state_topic, i + 1);
		
		if(config->turn_on_at_boot) {
     		mqtt->publish(msg, "1", true);
     	} else {
     		mqtt->publish(msg, "0", true);
     	}

     	LOG_PRINTF(INFO, "Published Initial State to: %s", msg);

     	snprintf(msg, 150, config->mqtt_topic, i + 1);
     	mqtt->subscribe(msg);

     	LOG_PRINTF(INFO, "Subscribed to action topic: %s", msg);
    }

    for(int i = 0; i< sizeof(button_map) / sizeof(uint8_t); i++) {
    	pinMode(button_map[i], INPUT);
    	LOG_PRINTF(INFO, "Turning on GPIO %d as Button Input", button_map[i]);
    }

    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);

}

void mqttCallback(char *topic, byte *payload, unsigned int length)
{
    unsigned int switchPin = 0;
    unsigned int switchId = 0;
    unsigned int switchState = 0;

    app_config_t *config;
    char msg[150];

    config = (app_config_t *)configManager->getConfig();

    LOG_PRINTF(DEBUG, "MQTT Callback Triggered. Topic: %s\n", topic);

    for(int i = 0; i < sizeof(switch_map) / sizeof(uint8_t); i++) {
    	  snprintf(msg, 150, config->mqtt_topic, i + 1);

    	  if(strcmp(topic, msg) == 0) {
    		  switchPin = switch_map[i];
    		  switchId = i + 1;
    	  }
    }

    if(switchPin <= 0) {
      return;
    }

    LOG_PRINTF(DEBUG, "Processing request for switch %d on pin %d\n", switchId, switchPin);

    if((char)payload[0] == '1') {
      digitalWrite(switchPin, HIGH);
      switchState = HIGH;
    } else if((char)payload[0] == '0') {
      digitalWrite(switchPin, LOW);
      switchState = LOW;
    }

    snprintf(msg, 150, config->mqtt_state_topic, switchId);

    mqtt->publish(msg, switchState == HIGH ? "1" : "0", true);
}

void setupMQTT()
{
	mqttManager = new CoogleIOT_MQTT;
	mqttManager->setLogger(_ciot_log);
	mqttManager->setConfigManager(configManager);
	mqttManager->setWifiManager(WiFiManager);
	mqttManager->initialize();
	mqttManager->setClientId(APP_NAME);
	mqttManager->setConnectCallback(setupRelays);
	mqttManager->connect();

	mqtt = mqttManager->getClient();
	mqtt->setCallback(mqttCallback);

}

void onNewFirmware()
{
	LOG_INFO("New Firmware available");
	LOG_INFO("Current Firmware Details");
	LOG_PRINTF(INFO, APP_NAME " v%s (%s) (built: %s %s)\r\n", _BuildInfo.src_version, _BuildInfo.env_version, _BuildInfo.date, _BuildInfo.time);

	restart = true;
}

void onNTPReady()
{
	setupOTA();
	ota_ready = true;
}

void setupNTP()
{
    NTPManager = new CoogleIOT_NTP;
	NTPManager->setLogger(_ciot_log);
    NTPManager->setWifiManager(WiFiManager);
    NTPManager->setReadyCallback(onNTPReady);
    NTPManager->initialize();

}

void setupLogging()
{
	setupSerial();
    _ciot_log = new CoogleIOT_Logger(&Serial);
    _ciot_log->initialize();

}

bool onParseConfig(DynamicJsonDocument& doc) {
	JsonObject app;

	if(!doc["app"].is<JsonObject>()) {
		LOG_ERROR("No application configuration found");
		return false;
	}

	app = doc["app"].as<JsonObject>();

	if(app["mqtt_topic"].is<const char *>()) {
		strlcpy(app_config->mqtt_topic, app["mqtt_topic"] | "", sizeof(app_config->mqtt_topic));

		LOG_PRINTF(INFO, "Action Topic: %s", app_config->mqtt_topic);
	}

	if(app["mqtt_state_topic"].is<const char *>()) {
		strlcpy(app_config->mqtt_state_topic, app["mqtt_state_topic"] | "", sizeof(app_config->mqtt_state_topic));
		LOG_PRINTF(INFO, "State Topic: %s", app_config->mqtt_state_topic);

	}

	if(app["turn_on_at_boot"].is<bool>()) {
		app_config->turn_on_at_boot = app["turn_on_at_boot"].as<bool>();

		if(app_config->turn_on_at_boot) {
			LOG_INFO("Turning on Relays by Default");
		} else {
			LOG_INFO("Turning off Relays by Default");
		}
	}

	if(app["debounce_time"].is<uint16_t>()) {
		app_config->debounce_time = app["debounce_time"].as<uint16_t>();
		LOG_PRINTF(INFO, "Setting button debounce time to %d", app_config->debounce_time);
	}

	return true;
}

void setupConfig()
{
	char *t;
	app_config = (app_config_t *)os_zalloc(sizeof(app_config_t));

	configManager = CoogleIOT_Config::getInstance();
	configManager->setJsonConfigSize(JSON_OBJECT_SIZE(1) + 3*JSON_OBJECT_SIZE(2) + 2*JSON_OBJECT_SIZE(4) + JSON_OBJECT_SIZE(6) + 687);
	configManager->setLogger(_ciot_log);
	configManager->setConfigStruct((coogleiot_config_base_t *)app_config);
	configManager->setParseCallback(onParseConfig);
	configManager->initialize();
}


void setupWiFi()
{
    WiFiManager = new CoogleIOT_Wifi;
    WiFiManager->setLogger(_ciot_log);
    WiFiManager->setConfigManager(configManager);

    WiFiManager->initialize();
}

void verifyUpgrade()
{
	otaManager->verifyOTAComplete();
	restart = true;
}

void setupOTA()
{
	otaManager = new CoogleIOT_OTA;
	otaManager->setLogger(_ciot_log);
	otaManager->setWifiManager(WiFiManager);
	otaManager->setNTPManager(NTPManager);
	otaManager->setCurrentVersion(_BuildInfo.src_version);
	otaManager->setConfigManager(configManager);
	otaManager->setOTACompleteCallback(onNewFirmware);
	otaManager->setUpgradeVerifyCallback(verifyUpgrade);
	otaManager->useSSL(false);
	otaManager->initialize();
}

void logSetupInfo()
{
	FSInfo fs_info;
	LOG_PRINTF(INFO, APP_NAME " v%s (%s) (built: %s %s)\r\n", _BuildInfo.src_version, _BuildInfo.env_version, _BuildInfo.date, _BuildInfo.time);

	if(!SPIFFS.begin()) {
		LOG_ERROR("Failed to start SPIFFS file system!");
	} else {
		SPIFFS.info(fs_info);
		LOG_INFO("SPIFFS File System");
		LOG_INFO("-=-=-=-=-=-=-=-=-=-");
		LOG_PRINTF(INFO, "Total Size: %d byte(s)\nUsed: %d byte(s)\nAvailable: ~ %d byte(s)", fs_info.totalBytes, fs_info.usedBytes, fs_info.totalBytes - fs_info.usedBytes);
	}
}

void setup()
{
    randomSeed(micros());

    setupLogging();
    setupConfig();

	logSetupInfo();

    setupWiFi();
    setupNTP();
    setupMQTT();

    // Give the logger an NTP Manager so it can record timestamps with logs
    _ciot_log->setNTPManager(NTPManager);

}

void loop()
{
	char msg[150];
	app_config_t *config;

	if(restart) {
		ESP.restart();
		return;
	}

	WiFiManager->loop();
	NTPManager->loop();
	mqttManager->loop();

	if(ota_ready) {
		otaManager->loop();
	}

	if(!configManager) {
		return;
	}

	if(!configManager->loaded) {
		return;
	}

	config = (app_config_t *)configManager->getConfig();

	for(int i = 0; i < sizeof(button_map) / sizeof(uint8_t); i++) {
		int currentReading;

		currentReading = digitalRead(button_map[i]);

		if(currentReading == LOW) {

			if(buttonReadings[i] == HIGH) {

				if((millis() - lastToggleTimes[i]) >= config->debounce_time) {

					// Build mqtt topic

					if(digitalRead(switch_map[i]) == HIGH) {
						digitalWrite(switch_map[i], LOW);

						// send mqtt topic
					} else {
						digitalWrite(switch_map[i], HIGH);

						// send mqtt topic
					}

					lastToggleTimes[i] = millis();

					// flash LED
				}
			}
		}

		buttonReadings[i] = currentReading;
	}
}
