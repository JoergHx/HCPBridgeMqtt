#ifndef LEGACYMQTT_H_
#define LEGACYMQTT_H_
#include <AsyncMqttClient.h>

#include "preferencesKeys.h"
#include "hoermann.h"
extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
}
TaskHandle_t mqttTask;
void mqttTaskFunc(void *parameter);

class MqttStrings {
   public:
    char availability_topic[64];
    char state_topic[64];
    char cmd_topic[64];
    char pos_topic[64];
    char setpos_topic[64];
    char lamp_topic[64];
    char door_topic[64];
    char vent_topic[64];
    char sensor_topic[64];
    char debug_topic[64];
    String st_availability_topic;
    String st_state_topic;
    String st_cmd_topic;
    String st_cmd_topic_var;
    String st_cmd_topic_subs;
    String st_pos_topic;
    String st_setpos_topic;
    String st_lamp_topic;
    String st_door_topic;
    String st_vent_topic;
    String st_sensor_topic;
    String st_debug_topic;
};

MqttStrings mqttStrings;

class MqttClient {
   private:
    char lastCommandTopic[64];
    char lastCommandPayload[64];
    volatile bool mqttConnected = false;
    TimerHandle_t mqttReconnectTimer;
    AsyncMqttClient mqttClient;
    MqttStrings mqttStrings;
    PreferenceHandler *prefHandler;
    Preferences *localPrefs;
    HoermannGarageEngine *hoermannEngine;
    void setuptMqttStrings(Preferences *prefs) {
        String ftopic = "hormann/" + prefs->getString(preference_gd_id);
        mqttStrings.st_availability_topic = ftopic + "/availability";
        mqttStrings.st_state_topic = ftopic + "/state";
        mqttStrings.st_cmd_topic = ftopic + "/command";
        mqttStrings.st_cmd_topic_var = mqttStrings.st_cmd_topic + "/%s";
        mqttStrings.st_cmd_topic_subs = mqttStrings.st_cmd_topic + "/#";
        mqttStrings.st_pos_topic = ftopic + "/position";
        mqttStrings.st_setpos_topic =
            mqttStrings.st_cmd_topic + "/set_position";
        mqttStrings.st_lamp_topic = mqttStrings.st_cmd_topic + "/lamp";
        mqttStrings.st_door_topic = mqttStrings.st_cmd_topic + "/door";
        mqttStrings.st_vent_topic = mqttStrings.st_cmd_topic + "/vent";
        mqttStrings.st_sensor_topic = ftopic + "/sensor";
        mqttStrings.st_debug_topic = ftopic + "/debug";

        strcpy(mqttStrings.availability_topic,
               mqttStrings.st_availability_topic.c_str());
        strcpy(mqttStrings.state_topic, mqttStrings.st_state_topic.c_str());
        strcpy(mqttStrings.cmd_topic, mqttStrings.st_cmd_topic.c_str());
        strcpy(mqttStrings.pos_topic, mqttStrings.st_pos_topic.c_str());
        strcpy(mqttStrings.setpos_topic, mqttStrings.st_setpos_topic.c_str());
        strcpy(mqttStrings.lamp_topic, mqttStrings.st_lamp_topic.c_str());
        strcpy(mqttStrings.door_topic, mqttStrings.st_door_topic.c_str());
        strcpy(mqttStrings.vent_topic, mqttStrings.st_vent_topic.c_str());
        strcpy(mqttStrings.sensor_topic, mqttStrings.st_sensor_topic.c_str());
        strcpy(mqttStrings.debug_topic, mqttStrings.st_debug_topic.c_str());
    }
    void switchLamp(bool on) { hoermannEngine->turnLight(on); }

    const char *ToHA(bool value) {
        if (value == true) {
            return HA_ON;
        }
        if (value == false) {
            return HA_OFF;
        }
        return "UNKNOWN";
    }
    void sendOnline() {
        mqttClient.publish(mqttStrings.availability_topic, 0, true, HA_ONLINE);
    }
    void onMqttMessage(char *topic, char *payload,
                       AsyncMqttClientMessageProperties properties, size_t len,
                       size_t index, size_t total) {
        // Note that payload is NOT a string; it contains raw data.
        // https://github.com/marvinroger/async-mqtt-client/blob/develop/docs/5.-Troubleshooting.md
        strcpy(lastCommandTopic, topic);
        strncpy(lastCommandPayload, payload, len);
        lastCommandPayload[len] = '\0';

        if (strcmp(topic, mqttStrings.lamp_topic) == 0) {
            if (strncmp(payload, HA_ON, len) == 0) {
                switchLamp(true);
            } else if (strncmp(payload, HA_OFF, len) == 0) {
                switchLamp(false);
            } else {
                hoermannEngine->toogleLight();
            }
        } else if (strcmp(mqttStrings.door_topic, topic) == 0 ||
                   strcmp(mqttStrings.vent_topic, topic) == 0) {
            if (strncmp(payload, HA_OPEN, len) == 0) {
                hoermannEngine->openDoor();
            } else if (strncmp(payload, HA_CLOSE, len) == 0) {
                hoermannEngine->closeDoor();
            } else if (strncmp(payload, HA_STOP, len) == 0) {
                hoermannEngine->stopDoor();
            } else if (strncmp(payload, HA_HALF, len) == 0) {
                hoermannEngine->halfPositionDoor();
            } else if (strncmp(payload, HA_VENT, len) == 0) {
                hoermannEngine->ventilationPositionDoor();
            }
        } else if (strcmp(mqttStrings.setpos_topic, topic) == 0) {
            hoermannEngine->setPosition(atoi(lastCommandPayload));
        }
    }

    void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
        mqttConnected = false;
        switch (reason) {
            case AsyncMqttClientDisconnectReason::TCP_DISCONNECTED:
                Serial.println(
                    "Disconnected from MQTT. reason : TCP_DISCONNECTED");
                break;
            case AsyncMqttClientDisconnectReason::
                MQTT_UNACCEPTABLE_PROTOCOL_VERSION:
                Serial.println(
                    "Disconnected from MQTT. reason : "
                    "MQTT_UNACCEPTABLE_PROTOCOL_VERSION");
                break;
            case AsyncMqttClientDisconnectReason::MQTT_IDENTIFIER_REJECTED:
                Serial.println(
                    "Disconnected from MQTT. reason : "
                    "MQTT_IDENTIFIER_REJECTED");
                break;
            case AsyncMqttClientDisconnectReason::MQTT_SERVER_UNAVAILABLE:
                Serial.println(
                    "Disconnected from MQTT. reason : MQTT_SERVER_UNAVAILABLE");
                break;
            case AsyncMqttClientDisconnectReason::ESP8266_NOT_ENOUGH_SPACE:
                Serial.println(
                    "Disconnected from MQTT. reason : "
                    "ESP8266_NOT_ENOUGH_SPACE");
                break;
            case AsyncMqttClientDisconnectReason::MQTT_MALFORMED_CREDENTIALS:
                Serial.println(
                    "Disconnected from MQTT. reason : "
                    "MQTT_MALFORMED_CREDENTIALS");
                break;
            case AsyncMqttClientDisconnectReason::MQTT_NOT_AUTHORIZED:
                Serial.println(
                    "Disconnected from MQTT. reason : MQTT_NOT_AUTHORIZED");
                break;
            case AsyncMqttClientDisconnectReason::TLS_BAD_FINGERPRINT:
                Serial.println(
                    "Disconnected from MQTT. reason :TLS_BAD_FINGERPRINT");
                break;
            default:
                break;
        }
    }
    void onMqttPublish(uint16_t packetId) {}

    void sendDiscoveryMessageForBinarySensor(const char name[],
                                             const char topic[],
                                             const char key[], const char off[],
                                             const char on[],
                                             const JsonDocument &device) {
        char full_topic[64];
        sprintf(full_topic, HA_DISCOVERY_BIN_SENSOR,
                localPrefs->getString(preference_gd_id), key);

        char uid[64];
        sprintf(uid, "%s_binary_sensor_%s",
                localPrefs->getString(preference_gd_id), key);

        char vtemp[64];
        sprintf(vtemp, "{{ value_json.%s }}", key);

        JsonDocument doc;

        doc["name"] = name;
        doc["state_topic"] = topic;
        doc["availability_topic"] = mqttStrings.availability_topic;
        doc["payload_available"] = HA_ONLINE;
        doc["payload_not_available"] = HA_OFFLINE;
        doc["unique_id"] = uid;
        doc["value_template"] = vtemp;
        doc["payload_on"] = on;
        doc["payload_off"] = off;
        doc["device"] = device;

        char payload[1024];
        serializeJson(doc, payload);
        //-//Serial.write(payload);
        mqttClient.publish(full_topic, 1, true, payload);
    }

    void sendDiscoveryMessageForAVSensor(const JsonDocument &device) {
        char full_topic[64];
        sprintf(full_topic, HA_DISCOVERY_AV_SENSOR,
                localPrefs->getString(preference_gd_id));

        char uid[64];
        sprintf(uid, "%s_sensor_availability",
                localPrefs->getString(preference_gd_id));
        JsonDocument doc;

        doc["name"] = localPrefs->getString(preference_gd_avail);
        doc["state_topic"] = mqttStrings.availability_topic;
        doc["unique_id"] = uid;
        doc["device"] = device;

        char payload[1024];
        serializeJson(doc, payload);
        //-//Serial.write(payload);
        mqttClient.publish(full_topic, 1, true, payload);
    }

    void sendDiscoveryMessageForSensor(const char name[], const char topic[],
                                       const char key[],
                                       const JsonDocument &device) {
        char full_topic[64];
        sprintf(full_topic, HA_DISCOVERY_SENSOR,
                localPrefs->getString(preference_gd_id), key);

        char uid[64];
        sprintf(uid, "%s_sensor_%s", localPrefs->getString(preference_gd_id),
                key);

        char vtemp[64];
        // small workaround to get the value as float
        if (key == "hum" || key == "temp" || key == "pres") {
            sprintf(vtemp, "{{ value_json.%s | float }}", key);
        } else {
            sprintf(vtemp, "{{ value_json.%s }}", key);
        }

        JsonDocument doc;

        doc["name"] = name;
        doc["state_topic"] = topic;
        doc["availability_topic"] = mqttStrings.availability_topic;
        doc["payload_available"] = HA_ONLINE;
        doc["payload_not_available"] = HA_OFFLINE;
        doc["unique_id"] = uid;
        doc["value_template"] = vtemp;
        doc["device"] = device;

        char payload[1024];
        serializeJson(doc, payload);
        //-//Serial.write(payload);
        mqttClient.publish(full_topic, 1, true, payload);
    }

    void sendDiscoveryMessageForDebug(const char name[], const char key[],
                                      const JsonDocument &device) {
        char command_topic[64];
        sprintf(command_topic, mqttStrings.st_cmd_topic_var.c_str(),
                mqttStrings.debug_topic);

        char full_topic[64];
        sprintf(full_topic, HA_DISCOVERY_TEXT,
                localPrefs->getString(preference_gd_id), key);

        char uid[64];
        sprintf(uid, "%s_text_%s", localPrefs->getString(preference_gd_id),
                key);

        char vtemp[64];
        sprintf(vtemp, "{{ value_json.%s }}", key);

        JsonDocument doc;

        doc["name"] = name;
        doc["state_topic"] = mqttStrings.debug_topic;
        doc["command_topic"] = command_topic;
        doc["availability_topic"] = mqttStrings.availability_topic;
        doc["payload_available"] = HA_ONLINE;
        doc["payload_not_available"] = HA_OFFLINE;
        doc["unique_id"] = uid;
        doc["value_template"] = vtemp;
        doc["device"] = device;

        char payload[1024];
        serializeJson(doc, payload);
        //-//Serial.write(payload);
        mqttClient.publish(full_topic, 1, true, payload);
    }

    void sendDiscoveryMessageForSwitch(const char name[],
                                       const char discovery[],
                                       const char topic[], const char off[],
                                       const char on[], const char icon[],
                                       const JsonDocument &device,
                                       bool optimistic = false) {
        char command_topic[64];
        sprintf(command_topic, mqttStrings.st_cmd_topic_var.c_str(), topic);

        char full_topic[64];
        sprintf(full_topic, discovery, localPrefs->getString(preference_gd_id),
                topic);

        char value_template[64];
        sprintf(value_template, "{{ value_json.%s }}", topic);

        char uid[64];
        if (discovery == HA_DISCOVERY_LIGHT) {
            sprintf(uid, "%s_light_%s", localPrefs->getString(preference_gd_id),
                    topic);
        } else {
            sprintf(uid, "%s_switch_%s",
                    localPrefs->getString(preference_gd_id), topic);
        }

        JsonDocument doc;

        doc["name"] = name;
        doc["state_topic"] = mqttStrings.state_topic;
        doc["command_topic"] = command_topic;
        doc["payload_on"] = on;
        doc["payload_off"] = off;
        doc["icon"] = icon;
        doc["availability_topic"] = mqttStrings.availability_topic;
        doc["payload_available"] = HA_ONLINE;
        doc["payload_not_available"] = HA_OFFLINE;
        doc["unique_id"] = uid;
        doc["value_template"] = value_template;
        doc["optimistic"] = optimistic;
        doc["device"] = device;

        char payload[1024];
        serializeJson(doc, payload);
        //-//Serial.write(payload);
        mqttClient.publish(full_topic, 1, true, payload);
    }

    void sendDiscoveryMessageForCover(const char name[], const char topic[],
                                      const JsonDocument &device) {
        char command_topic[64];
        sprintf(command_topic, mqttStrings.st_cmd_topic_var.c_str(), topic);

        char full_topic[64];
        sprintf(full_topic, HA_DISCOVERY_COVER,
                localPrefs->getString(preference_gd_id), topic);

        char uid[64];
        sprintf(uid, "%s_cover_%s", localPrefs->getString(preference_gd_id),
                topic);

        JsonDocument doc;
        // if it didn't work try without state topic.
        doc["name"] = name;
        doc["state_topic"] = mqttStrings.state_topic;
        doc["command_topic"] = command_topic;
        doc["position_topic"] = mqttStrings.pos_topic;
        doc["set_position_topic"] = mqttStrings.setpos_topic;
        doc["position_open"] = 100;
        doc["position_closed"] = 0;

        doc["payload_open"] = HA_OPEN;
        doc["payload_close"] = HA_CLOSE;
        doc["payload_stop"] = HA_STOP;
#ifdef AlignToOpenHab
        doc["value_template"] = "{{ value_json.doorposition }}";
#else
        doc["value_template"] = "{{ value_json.doorstate }}";
#endif
        doc["state_open"] = HA_OPEN;
        doc["state_opening"] = HA_OPENING;
        doc["state_closed"] = HA_CLOSED;
        doc["state_closing"] = HA_CLOSING;
        doc["state_stopped"] = HA_STOP;
        doc["availability_topic"] = mqttStrings.availability_topic;
        doc["payload_available"] = HA_ONLINE;
        doc["payload_not_available"] = HA_OFFLINE;
        doc["unique_id"] = uid;
        doc["device_class"] = "garage";
        doc["device"] = device;

        char payload[1024];
        serializeJson(doc, payload);
        //-//Serial.write(payload);
        mqttClient.publish(full_topic, 1, true, payload);
    }

    void sendDiscoveryMessage() {
        // declare json object here for device instead of creating in each
        // methode. 150 bytes should be enough
        JsonDocument device;
        device["identifiers"] = localPrefs->getString(preference_gd_name);
        device["name"] = localPrefs->getString(preference_gd_name);
        device["sw_version"] = HA_VERSION;
        device["model"] = "Garage Door";
        device["manufacturer"] = "Hörmann";

        sendDiscoveryMessageForAVSensor(device);
        // not able to get it working sending the discovery message for light.
        sendDiscoveryMessageForSwitch(
            localPrefs->getString(preference_gd_light).c_str(),
            HA_DISCOVERY_SWITCH, "lamp", HA_OFF, HA_ON, "mdi:lightbulb",
            device);
        sendDiscoveryMessageForBinarySensor(
            localPrefs->getString(preference_gd_light).c_str(),
            mqttStrings.state_topic, "lamp", HA_OFF, HA_ON, device);
        sendDiscoveryMessageForSwitch(
            localPrefs->getString(preference_gd_vent).c_str(),
            HA_DISCOVERY_SWITCH, "vent", HA_CLOSE, HA_VENT, "mdi:air-filter",
            device);
        sendDiscoveryMessageForCover(
            localPrefs->getString(preference_gd_name).c_str(), "door", device);

        sendDiscoveryMessageForSensor(
            localPrefs->getString(preference_gd_status).c_str(),
            mqttStrings.state_topic, "doorstate", device);
        sendDiscoveryMessageForSensor(
            localPrefs->getString(preference_gd_det_status).c_str(),
            mqttStrings.state_topic, "detailedState", device);
        sendDiscoveryMessageForSensor(
            localPrefs->getString(preference_gd_position).c_str(),
            mqttStrings.state_topic, "doorposition", device);
#ifdef SENSORS
#if defined(USE_BME)
        sendDiscoveryMessageForSensor(
            localPrefs->getString(preference_gs_temp).c_str(),
            mqttStrings.sensor_topic, "temp", device);
        sendDiscoveryMessageForSensor(
            localPrefs->getString(preference_gs_hum).c_str(),
            mqttStrings.sensor_topic, "hum", device);
        sendDiscoveryMessageForSensor(
            localPrefs->getString(preference_gs_pres).c_str(),
            mqttStrings.sensor_topic, "pres", device);
#elif defined(USE_DS18X20)
        sendDiscoveryMessageForSensor(
            localPrefs->getString(preference_gs_temp).c_str(),
            mqttStrings.sensor_topic, "temp", device);
#endif
#if defined(USE_HCSR04)
        sendDiscoveryMessageForSensor(
            localPrefs->getString(preference_gs_free_dist).c_str(),
            mqttStrings.sensor_topic, "dist", device);
        sendDiscoveryMessageForBinarySensor(
            localPrefs->getString(preference_gs_park_avail).c_str(),
            mqttStrings.sensor_topic, "free", HA_OFF, HA_ON, device);
#endif
#if defined(USE_DHT22)
        sendDiscoveryMessageForSensor(
            localPrefs->getString(preference_gs_temp).c_str(),
            mqttStrings.sensor_topic, "temp", device);
        sendDiscoveryMessageForSensor(
            localPrefs->getString(preference_gs_hum).c_str(),
            mqttStrings.sensor_topic, "hum", device);
#endif
#if defined(USE_HCSR501)
        sendDiscoveryMessageForBinarySensor(
            localPrefs->getString(preference_sensor_sr501).c_str(),
            mqttStrings.sensor_topic, "motion", HA_OFF, HA_ON, device);
#endif
#endif
#ifdef DEBUG
        sendDiscoveryMessageForDebug(
            localPrefs->getString(preference_gd_debug).c_str(), "debug",
            device);
        sendDiscoveryMessageForDebug(
            localPrefs->getString(preference_gd_debug_restart).c_str(),
            "reset-reason", device);
#endif
    }

    void onMqttConnect(bool sessionPresent) {
        Serial.println("Function on mqtt connect.");
        mqttConnected = true;
        stopReconnectTimer();
        sendOnline();
        mqttClient.subscribe(mqttStrings.st_cmd_topic_subs.c_str(), 1);
        updateDoorStatus(true);
        updateSensors(true);
        sendDiscoveryMessage();
#ifdef DEBUG
        if (boot_Flag) {
            int i = esp_reset_reason();
            char val[3];
            sprintf(val, "%i", i);
            sendDebug("ResetReason", val);
            boot_Flag = false;
        }
#endif
    }

   public:
    void setup(PreferenceHandler *prefHandler,
               HoermannGarageEngine *RefHoermannEngine) {
        auto onTimer = [](TimerHandle_t hTmr) {
            MqttClient *mm = static_cast<MqttClient *>(
                pvTimerGetTimerID(hTmr));  // Retrieve the pointer to class
            assert(mm);                    // Sanity check
            mm->connect(hTmr);             // Forward to the real callback
        };

        mqttReconnectTimer =
            xTimerCreate("mqttTimer", pdMS_TO_TICKS(2000), pdFALSE,
                         static_cast<void *>(this), onTimer);

        setuptMqttStrings(prefHandler->getPreferences());
        this->localPrefs = prefHandler->getPreferences();
        this->hoermannEngine = RefHoermannEngine;
        mqttClient.onConnect(
            std::bind(&MqttClient::onMqttConnect, this, std::placeholders::_1));
        mqttClient.onDisconnect(std::bind(&MqttClient::onMqttDisconnect, this,
                                          std::placeholders::_1));
        mqttClient.onMessage(std::bind(
            &MqttClient::onMqttMessage, this, std::placeholders::_1,
            std::placeholders::_2, std::placeholders::_3, std::placeholders::_4,
            std::placeholders::_5, std::placeholders::_6));
        mqttClient.onPublish(
            std::bind(&MqttClient::onMqttPublish, this, std::placeholders::_1));
        mqttClient.setServer(prefHandler->getPreferencesCache()->mqtt_server,
                             localPrefs->getInt(preference_mqtt_server_port));
        mqttClient.setCredentials(
            prefHandler->getPreferencesCache()->mqtt_user,
            prefHandler->getPreferencesCache()->mqtt_password);
        setWill();

        xTaskCreatePinnedToCore(
            mqttTaskFunc, /* Function to implement the task */
            "MqttTask",   /* Name of the task */
            10000,        /* Stack size in words */
            NULL,         /* Task input parameter */
            // 1,  /* Priority of the task */
            configMAX_PRIORITIES - 3, &mqttTask, /* Task handle. */
            0); /* Core where the task should run */
    }
    char *getLastCommandTopic() { return lastCommandTopic; }
    char *getLastCommandPayload() { return lastCommandPayload; }
    bool connected() { return mqttClient.connected(); }
    void connect(TimerHandle_t hTmr) {
        mqttClient.connect(); 
        }
    void connect() {
        mqttClient.connect(); 
    }
    void stopReconnectTimer() {
        xTimerStop(mqttReconnectTimer, 0);  // ensure we don't reconnect to MQTT
                                            // while reconnecting to Wi-Fi
    }
    void startReconnectTimer() {
        if (!mqttConnected) {
            xTimerStart(mqttReconnectTimer, 0);
        }
    }
    void updateDoorStatus(bool forceUpate = false) {
        // onyl send updates when state changed
        if (hoermannEngine->state->changed || forceUpate) {
            hoermannEngine->state->clearChanged();
            JsonDocument doc;
            char payload[1024];
            const char *venting = HA_CLOSE;

            doc["valid"] = hoermannEngine->state->valid;
            doc["doorposition"] =
                (int)(hoermannEngine->state->currentPosition * 100);
            doc["lamp"] = ToHA(hoermannEngine->state->lightOn);
            doc["doorstate"] = hoermannEngine->state->coverState;
            doc["detailedState"] = hoermannEngine->state->translatedState;
            if (hoermannEngine->state->translatedState == HA_VENT) {
                venting = HA_VENT;
            }
            doc["vent"] = venting;

            serializeJson(doc, payload);
            mqttClient.publish(mqttStrings.state_topic, 1, true, payload);

            sprintf(payload, "%d",
                    (int)(hoermannEngine->state->currentPosition * 100));
            mqttClient.publish(mqttStrings.pos_topic, 1, true, payload);
        }
    }
    void updateSensors(bool forceUpate = false) {}

    void setWill() {
        mqttClient.setWill(mqttStrings.availability_topic, 0, true, HA_OFFLINE);
    }

    void sendDebug(char *key, String value) {
        JsonDocument doc;
        char payload[1024];
        doc["reset-reason"] = esp_reset_reason();
        doc["debug"] = hoermannEngine->state->debugMessage;
        serializeJson(doc, payload);
        mqttClient.publish(mqttStrings.debug_topic, 0, false,
                           payload);  // uint16_t publish(const char* topic,
                                      // uint8_t qos, bool retain, const char*
                                      // payload = nullptr, size_t length = 0)
    }
};
MqttClient *legacyMqttClient = new MqttClient();

void mqttTaskFunc(void *parameter) {
    while (true) {
        if (legacyMqttClient->connected()) {
            legacyMqttClient->updateDoorStatus(false);
            legacyMqttClient->updateSensors(false);
#ifdef DEBUG
            if (hoermannEngine->state->debMessage) {
                hoermannEngine->state->clearDebug();
                sendDebug();
            }
#endif
        }
        vTaskDelay(READ_DELAY);  // delay task xxx ms
    }
}
#endif