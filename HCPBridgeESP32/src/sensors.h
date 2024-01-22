#ifdef USE_DS18X20
#include <OneWire.h>
#include <DallasTemperature.h>
#endif

#ifdef USE_BME
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#endif

#ifdef USE_DHT22
#include <Adafruit_Sensor.h>
#include <DHT.h>
#endif

TaskHandle_t sensorTask;
class SensorHandler
{
private:
  // sensors
  bool new_sensor_data = false;
#ifdef SENSORS
  int sensor_prox_tresh = 0;
  float sensor_temp_thresh = 0;
  int sensor_hum_thresh = 0;
  int sensor_pres_thresh = 0;
  int sensor_last_update = 0;
  int sensor_force_update_intervall = 7200000; // force sending sensor updates after amount of time since last update
#endif

#ifdef USE_DS18X20
  // Setup a oneWire instance to communicate with any OneWire devices
  DallasTemperature *ds18x20 = nullptr;
  float ds18x20_temp = -99.99;
  float ds18x20_last_temp = -99.99;
  int ds18x20_pin = 0;
#endif
#ifdef USE_BME
  TwoWire I2CBME = TwoWire(0);
  Adafruit_BME280 bme;
  unsigned bme_status;
  float bme_temp = -99.99;
  float bme_last_temp = -99.99;
  float bme_hum = -99.99;
  float bme_last_hum = -99.99;
  float bme_pres = -99.99;
  float bme_last_pres = -99.99;
  int i2c_onoffpin = 0;
  int i2c_sdapin = 0;
  int i2c_sclpin = 0;
#endif

#ifdef USE_HCSR04
  long hcsr04_duration = -99.99;
  int hcsr04_distanceCm = 0;
  int hcsr04_lastdistanceCm = 0;
  int hcsr04_maxdistanceCm = 150;
  bool hcsr04_park_available = false;
  bool hcsr04_lastpark_available = false;
  int hcsr04_tgpin = 0;
  int hscr04_ecpin = 0;
#endif

#ifdef USE_DHT22
  DHT *dht = nullptr;
  float dht22_temp = -99.99;
  float dht22_last_temp = -99.99;
  float dht22_hum = -99.99;
  float dht22_last_hum = -99.99;
  int dht_vcc_pin = 0;
#endif

#ifdef USE_HCSR501
  int hcsr501stat = 0;
  bool hcsr501_laststat = false;
#endif
void SensorCheck(void *parameter){
  while(true){
    // handle motion sensor at first and send state immediately. Do not 
    // use updateSensors to avoid unneccessary polling of the other sensors
    #ifdef USE_HCSR501
      hcsr501stat = digitalRead(SR501PIN);
      if (hcsr501stat != hcsr501_laststat) {
        hcsr501_laststat = hcsr501stat;
        JsonDocument doc;
        char payload[1024];
        if (hcsr501stat) {
          doc["motion"] = HA_ON;
        }
        else {
          doc["motion"] = HA_OFF;
        }
        serializeJson(doc, payload);
        mqttClient.publish(mqttStrings.sensor_topic, 1, true, payload);
      }
    #endif
    #ifdef USE_DS18X20
      ds18x20_temp = ds18x20->getTempCByIndex(0);
      if (abs(ds18x20_temp-ds18x20_last_temp) >= sensor_temp_thresh){
        ds18x20_last_temp = ds18x20_temp;
        new_sensor_data = true;
      }
    #endif
    #ifdef USE_BME
      if (digitalRead(i2c_onoffpin) == LOW) {
        digitalWrite(i2c_onoffpin, HIGH);   // activate sensor
        sleep(10);
        I2CBME.begin(i2c_sdapin, i2c_sclpin);   // https://randomnerdtutorials.com/esp32-i2c-communication-arduino-ide/
        bme_status = bme.begin(0x76, &I2CBME);  // check sensor. adreess can be 0x76 or 0x77
        //bme_status = bme.begin();  // check sensor. adreess can be 0x76 or 0x77
      }
      if (!bme_status) {
        JsonDocument doc;    //2048 needed because of BME280 float values!
        // char payload[1024];
        // doc["bme_status"] = "Could not find a valid BME280 sensor!";   // see: https://github.com/adafruit/Adafruit_BME280_Library/blob/master/examples/bme280test/bme280test.ino#L49
        // serializeJson(doc, payload);
        // mqttClient.publish(SENSOR_TOPIC, 0, false, payload);  //uint16_t publish(const char* topic, uint8_t qos, bool retain, const char* payload = nullptr, size_t length = 0)
        digitalWrite(i2c_onoffpin, LOW);      // deactivate sensor
      } else {
        bme_temp = bme.readTemperature();   // round float
        bme_hum = bme.readHumidity();
        bme_pres = bme.readPressure()/100;  // convert from pascal to mbar
        if (bme_hum < 99.9){                   // I2C hung up ...
          if (abs(bme_temp-bme_last_temp) >= sensor_temp_thresh || abs(bme_hum-bme_last_hum) >= sensor_hum_thresh || abs(bme_pres-bme_last_pres) >= sensor_pres_thresh){
            bme_last_temp = bme_temp;
            bme_last_hum = bme_hum;
            bme_last_pres = bme_pres;
            new_sensor_data = true;
          }
        } else {
          digitalWrite(i2c_onoffpin, LOW);      // deactivate sensor
        }
      }
    #endif
    #ifdef USE_HCSR04

        // Clears the trigPin
        digitalWrite(hcsr04_tgpin, LOW);
        delayMicroseconds(2);
        // Sets the trigPin on HIGH state for 10 micro seconds
        digitalWrite(hcsr04_tgpin, HIGH);
        delayMicroseconds(10);
        digitalWrite(hcsr04_tgpin, LOW);
        // Reads the echoPin, returns the sound wave travel time in microseconds
        hcsr04_duration = pulseIn(hscr04_ecpin, HIGH);
        // Calculate the distance
        hcsr04_distanceCm = hcsr04_duration * SOUND_SPEED/2;
        if (hcsr04_distanceCm > hcsr04_maxdistanceCm) {
          // set new Max
          hcsr04_maxdistanceCm = hcsr04_distanceCm;
        }
        if ((hcsr04_distanceCm + sensor_prox_tresh) > hcsr04_maxdistanceCm ){
          hcsr04_park_available = true;
        } else {
          hcsr04_park_available = false;
        }
        if (abs(hcsr04_distanceCm-hcsr04_lastdistanceCm) >= sensor_prox_tresh || hcsr04_park_available != hcsr04_lastpark_available ){
          hcsr04_lastdistanceCm = hcsr04_distanceCm;
          hcsr04_lastpark_available = hcsr04_park_available;
          new_sensor_data = true;
        }
    #endif
    #ifdef USE_DHT22
      pinMode(dht_vcc_pin, OUTPUT);
      digitalWrite(dht_vcc_pin, HIGH);
      dht->begin();

      dht22_temp = dht->readTemperature();
      dht22_hum = dht->readHumidity();

      if (abs(dht22_temp) >= sensor_temp_thresh || abs(dht22_hum) >= sensor_hum_thresh){
        dht22_last_temp = dht22_temp;
        dht22_last_hum = dht22_hum;
        new_sensor_data = true;
      }
    #endif
    vTaskDelay(localPrefs->getInt(preference_query_interval_sensors)*1000);     // delay task xxx ms if statemachine had nothing to do
    //vTaskDelay(SENSE_PERIOD);     // TODO take from Preferences
  }
}
public:
  void setup()
  {

#ifdef SENSORS
    sensor_prox_tresh = localPrefs->getInt(preference_sensor_prox_treshold);
    sensor_temp_thresh = localPrefs->getInt(preference_sensor_prox_treshold);
    sensor_hum_thresh = localPrefs->getInt(preference_sensor_prox_treshold);
    sensor_pres_thresh = localPrefs->getInt(preference_sensor_prox_treshold);
#ifdef USE_DS18X20
    ds18x20_pin = localPrefs->getInt(preference_sensor_ds18x20_pin);
    OneWire oneWire(ds18x20_pin);
    static DallasTemperature static_ds18x20(&oneWire);
    // save its address.
    ds18x20 = &static_ds18x20;
    ds18x20->begin();
#endif
#ifdef USE_BME
    i2c_onoffpin = localPrefs->getInt(preference_sensor_i2c_on_off);
    i2c_sdapin = localPrefs->getInt(preference_sensor_i2c_sda);
    i2c_sclpin = localPrefs->getInt(preference_sensor_i2c_scl);
    pinMode(i2c_onoffpin, OUTPUT);
    I2CBME.begin(i2c_sdapin, i2c_sclpin);  // https://randomnerdtutorials.com/esp32-i2c-communication-arduino-ide/
    bme_status = bme.begin(0x76, &I2CBME); // check sensor. adreess can be 0x76 or 0x77
    // bme_status = bme.begin();  // check sensor. adreess can be 0x76 or 0x77
#endif
#ifdef USE_HCSR04
    hcsr04_tgpin = localPrefs->getInt(preference_sensor_sr04_trigpin);
    hscr04_ecpin = localPrefs->getInt(preference_sensor_sr04_echopin);
    hcsr04_maxdistanceCm = localPrefs->getInt(preference_sensor_sr04_max_dist);
    pinMode(hcsr04_tgpin, OUTPUT); // Sets the trigPin as an Output
    pinMode(hscr04_ecpin, INPUT);  // Sets the echoPin as an Input
#endif
#ifdef USE_HCSR501
    pinMode(SR501PIN, INPUT);                 // Sets the trigPin as an Output
    hcsr501_laststat = digitalRead(SR501PIN); // read first state of sensor
#endif
#ifdef USE_DHT22
    dht_vcc_pin = localPrefs->getInt(preference_sensor_dht_vcc_pin);
    static DHT static_dht(dht_vcc_pin, DHTTYPE);
    // save its address.
    dht = &static_dht;
    pinMode(dht_vcc_pin, OUTPUT);
    digitalWrite(dht_vcc_pin, HIGH);
    dht->begin();
#endif

    xTaskCreatePinnedToCore(
        SensorCheck,  /* Function to implement the task */
        "SensorTask", /* Name of the task */
        10000,        /* Stack size in words */
        NULL,         /* Task input parameter */
        // 1,  /* Priority of the task */
        configMAX_PRIORITIES,
        &sensorTask, /* Task handle. */
        0);          /* Core where the task should run */
#endif
  }

};