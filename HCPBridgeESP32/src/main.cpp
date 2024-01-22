#include <Arduino.h>
#include <Esp.h>
#include <AsyncElegantOTA.h>
#include <ESPAsyncWebServer.h>
#include "AsyncJson.h"
extern "C" {
	#include "freertos/FreeRTOS.h"
	#include "freertos/timers.h"
}
#include "ArduinoJson.h"

#include "hoermann.h"
#include "preferencesKeys.h"
#include "legacyMqtt.h"
#include "../../WebUI/index_html.h"


// webserver on port 80
AsyncWebServer server(80);

// sensors
bool new_sensor_data = false;

PreferenceHandler prefHandler;
Preferences *localPrefs = nullptr;

TimerHandle_t wifiReconnectTimer;

unsigned long resetButtonTimePressed = 0l;
TimerHandle_t resetTimer;

#ifdef DEBUG
  bool boot_Flag = true;
#endif


void IRAM_ATTR reset_button_change(){
  if (digitalRead(0) == 0)
  {
    // Pressed
    resetButtonTimePressed = millis();
  } else {
    // unpressed
    unsigned long timeNow = millis();
    int timeInSecs = (timeNow - resetButtonTimePressed) / 1000;
    if (timeInSecs > 5)
    {
      xTimerStart(resetTimer, 0);
    }
    resetButtonTimePressed = 0;
  }
}

void resetPreferences()
{
  xTimerStop(resetTimer, 0);
  Serial.println("Resetting config...");
  prefHandler.resetPreferences();
}

void connectToWifi() {
  /*if (localPrefs->getBool(preference_wifi_ap_mode))
  {
    Serial.println("WIFI AP mode enabled, set Hostname");
    WiFi.softAP(prefHandler.getPreferencesCache()->hostname);
    return;
  }*/
  if (localPrefs->getString(preference_wifi_ssid) != "")
  {
    Serial.println("Connecting to Wi-Fi...");
    WiFi.begin(localPrefs->getString(preference_wifi_ssid).c_str(), localPrefs->getString(preference_wifi_password).c_str());
  } else
  {
    Serial.println("No WiFi Client enabled");
  }

  //Serial.println("Connecting to Wi-Fi...");
  //this disocnnect should not be necessary as we restart the esp after changing form AP mode to Station mode.
  //WiFi.softAPdisconnect(true);  //stop AP, we now work as a wifi client

}

void updateDoorStatus(bool forceUpate = false)  {
  // onyl send updates when state changed
  if (hoermannEngine->state->changed || forceUpate){
    hoermannEngine->state->clearChanged();
    //call update methode of the mqtt client
  }
}

void updateSensors(bool forceUpate = false){
#ifdef SENSORS
    if (millis()-sensor_last_update >= sensor_force_update_intervall) {
      forceUpate = true;
    }
    
    if (new_sensor_data || forceUpate) {
      new_sensor_data = false;}
#endif
}


void WiFiEvent(WiFiEvent_t event) {
    String eventInfo = "No Info";

    switch (event) {
        case ARDUINO_EVENT_WIFI_READY: 
            eventInfo = "WiFi interface ready";
            break;
        case ARDUINO_EVENT_WIFI_SCAN_DONE:
            eventInfo = "Completed scan for access points";
            break;
        case ARDUINO_EVENT_WIFI_STA_START:
            eventInfo = "WiFi client started";
            break;
        case ARDUINO_EVENT_WIFI_STA_STOP:
            eventInfo = "WiFi clients stopped";
            break;
        case ARDUINO_EVENT_WIFI_STA_CONNECTED:
            eventInfo = "Connected to access point";
            break;
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            eventInfo = "Disconnected from WiFi access point";
            legacyMqttClient->stopReconnectTimer();
            xTimerStart(wifiReconnectTimer, 0);
            break;
        case ARDUINO_EVENT_WIFI_STA_AUTHMODE_CHANGE:
            eventInfo = "Authentication mode of access point has changed";
            break;
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            eventInfo = "Obtained IP address";
            xTimerStop(wifiReconnectTimer, 0); // stop in case it was started
            legacyMqttClient->startReconnectTimer();
            //Serial.println(WiFi.localIP());
            break;
        case ARDUINO_EVENT_WIFI_STA_LOST_IP:
            eventInfo = "Lost IP address and IP address is reset to 0";
            break;
        case ARDUINO_EVENT_WPS_ER_SUCCESS:
            eventInfo = "WiFi Protected Setup (WPS): succeeded in enrollee mode";
            break;
        case ARDUINO_EVENT_WPS_ER_FAILED:
            eventInfo = "WiFi Protected Setup (WPS): failed in enrollee mode";
            break;
        case ARDUINO_EVENT_WPS_ER_TIMEOUT:
            eventInfo = "WiFi Protected Setup (WPS): timeout in enrollee mode";
            break;
        case ARDUINO_EVENT_WPS_ER_PIN:
            eventInfo = "WiFi Protected Setup (WPS): pin code in enrollee mode";
            break;
        case ARDUINO_EVENT_WIFI_AP_START:
            eventInfo = "WiFi access point started";
            break;
        case ARDUINO_EVENT_WIFI_AP_STOP:
            eventInfo = "WiFi access point  stopped";
            break;
        case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
            eventInfo = "Client connected";
            break;
        case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
            eventInfo = "Client disconnected";
            break;
        case ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED:
            eventInfo = "Assigned IP address to client";
            break;
        case ARDUINO_EVENT_WIFI_AP_PROBEREQRECVED:
            eventInfo = "Received probe request";
            break;
        case ARDUINO_EVENT_WIFI_AP_GOT_IP6:
            eventInfo = "AP IPv6 is preferred";
            break;
        case ARDUINO_EVENT_WIFI_STA_GOT_IP6:
            eventInfo = "STA IPv6 is preferred";
            break;
        case ARDUINO_EVENT_ETH_GOT_IP6:
            eventInfo = "Ethernet IPv6 is preferred";
            break;
        case ARDUINO_EVENT_ETH_START:
            eventInfo = "Ethernet started";
            break;
        case ARDUINO_EVENT_ETH_STOP:
            eventInfo = "Ethernet stopped";
            break;
        case ARDUINO_EVENT_ETH_CONNECTED:
            eventInfo = "Ethernet connected";
            break;
        case ARDUINO_EVENT_ETH_DISCONNECTED:
            eventInfo = "Ethernet disconnected";
            break;
        case ARDUINO_EVENT_ETH_GOT_IP:
            eventInfo = "Obtained IP address";
            break;
        default: break;
    }
    Serial.print("WIFI-Event: ");
    Serial.println(eventInfo);
}


// setup mcu
void setup()
{
  // Serial
  Serial.begin(9600);


  while (Serial.available()==0){
    //only continues if an input get received from serial.
    ;
  } 


  // setup preferences
  prefHandler.initPreferences();
  localPrefs = prefHandler.getPreferences();
  // setup modbus
  hoermannEngine->setup(localPrefs);

  //Add interrupts for Factoryreset over Boot button
  pinMode(0, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(0), reset_button_change, CHANGE);
  resetTimer = xTimerCreate("resetTimer", pdMS_TO_TICKS(10), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(resetPreferences));

  // setup wifi
  wifiReconnectTimer = xTimerCreate("wifiTimer", pdMS_TO_TICKS(2000), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(connectToWifi));
  WiFi.setHostname(prefHandler.getPreferencesCache()->hostname);
  if (localPrefs->getBool(preference_wifi_ap_mode)){
    Serial.println("WIFI AP enabled");
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(prefHandler.getPreferencesCache()->hostname);
    }
  else{
    WiFi.mode(WIFI_STA);  
  }
  
  WiFi.onEvent(WiFiEvent);

  connectToWifi();
  
  legacyMqttClient->setup(&prefHandler, hoermannEngine);


  // setup http server
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", index_html, sizeof(index_html));
              response->addHeader("Content-Encoding", "deflate");
              request->send(response); });

  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request){
              //const SHCIState &doorstate = emulator.getState();
              AsyncResponseStream *response = request->beginResponseStream("application/json");
              JsonDocument root;
              //response->print(hoermannEngine->state->toStatusJson());
              root["doorstate"] = hoermannEngine->state->translatedState;
              root["valid"] = hoermannEngine->state->valid;
              root["targetPosition"] = (int)(hoermannEngine->state->targetPosition * 100);
              root["currentPosition"] = (int)(hoermannEngine->state->currentPosition * 100);
              root["light"] = hoermannEngine->state->lightOn;
              root["state"] = hoermannEngine->state->state;
              root["busResponseAge"] = hoermannEngine->state->responseAge();
              root["lastModbusRespone"] = hoermannEngine->state->lastModbusRespone;
              #ifdef SENSORS
                #ifdef USE_DS18X20
                  root["temp"] = ds18x20_temp;
                #elif defined(USE_BME)
                  root["temp"] = bme_temp;
                #elif defined(USE_DHT22)
                  root["temp"] = dht22_temp;
                #endif
              #endif
              //root["debug"] = doorstate.reserved;
              root["lastCommandTopic"] = legacyMqttClient->getLastCommandTopic();
              root["lastCommandPayload"] = legacyMqttClient->getLastCommandPayload();
              serializeJson(root, *response);
              request->send(response); });

  server.on("/statush", HTTP_GET, [](AsyncWebServerRequest *request){
              AsyncResponseStream *response = request->beginResponseStream("application/json");
              response->print(hoermannEngine->state->toStatusJson());
              request->send(response); });

  server.on("/command", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              if (request->hasParam("action"))
              {
                int actionid = request->getParam("action")->value().toInt();
                switch (actionid)
                {
                case 0:
                  hoermannEngine->closeDoor();
                  break;
                case 1:
                  hoermannEngine->openDoor();
                  break;
                case 2:
                  hoermannEngine->stopDoor();
                  break;
                case 3:
                  hoermannEngine->ventilationPositionDoor();
                  break;
                case 4:
                  hoermannEngine->halfPositionDoor();
                  break;
                case 5:
                  hoermannEngine->toogleLight();
                  break;
                case 6:
                  Serial.println("restart...");
                  legacyMqttClient->setWill();
                  ESP.restart();
                  break;
                case 7:
                  if (request->hasParam("position"))
                    hoermannEngine->setPosition(request->getParam("position")->value().toInt());
                  break;
                default:
                  break;
                }
              }
              request->send(200, "text/plain", "OK");
              //const SHCIState &doorstate = emulator.getState();
              //onStatusChanged(doorstate);
              });

  server.on("/sysinfo", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              Serial.println("GET SYSINFO");
              AsyncResponseStream *response = request->beginResponseStream("application/json");
              JsonDocument root;
              root["freemem"] = ESP.getFreeHeap();
              root["hostname"] = WiFi.getHostname();
              root["ip"] = WiFi.localIP().toString();
              root["wifistatus"] = WiFi.status();
              root["mqttstatus"] = legacyMqttClient->connected();
              root["resetreason"] = esp_reset_reason();
              serializeJson(root, *response);

              request->send(response); });
  
  server.on("/config", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              Serial.println("GET CONFIG");
              AsyncResponseStream *response = request->beginResponseStream("application/json");
              JsonDocument conf;
              prefHandler.getConf(conf);
              serializeJson(conf, *response);
              request->send(response); });

  // load requestbody for json Post requests
  server.onRequestBody([](AsyncWebServerRequest * request, uint8_t *data, size_t len, size_t index, size_t total)
          {
          // Handle setting config request
          if (request->url() == "/config")
          {
            JsonDocument doc;
            deserializeJson(doc, data);
            prefHandler.saveConf(doc);

            request->send(200, "text/plain", "OK");
          } });
  server.on("/reset", HTTP_GET, [](AsyncWebServerRequest *request)
        {
          Serial.println("GET reset");
          AsyncResponseStream *response = request->beginResponseStream("application/json");
          JsonDocument root;
          root["reset"] = "OK";
          serializeJson(root, *response);
          request->send(response); 
          prefHandler.resetPreferences();
          });

  AsyncElegantOTA.begin(&server, OTA_USERNAME, OTA_PASSWD);

  server.begin();
}

// mainloop
void loop(){
}
