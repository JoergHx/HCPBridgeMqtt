#ifndef HASCLIENT_H_
#define HASCLIENT_H_

#include "hoermann.h"
#include "preferencesKeys.h"
#include <ArduinoHA.h> 
TaskHandle_t hasTask;

class hasClient {
   private:
    hasClient hasClient{};
   public:
    void setup(PreferenceHandler *prefHandler,
               HoermannGarageEngine *RefHoermannEngine) {
        xTaskCreatePinnedToCore(
            hasTaskFunc, /* Function to implement the task */
            "HasTask",   /* Name of the task */
            10000,       /* Stack size in words */
            NULL,        /* Task input parameter */
            // 1,  /* Priority of the task */
            configMAX_PRIORITIES - 3, &hasTask, /* Task handle. */
            0); /* Core where the task should run */
    }

    bool connected() { return mqttClient.connected(); }
    void connect() { mqttClient.connect(); }
    void updateDoorStatus(bool forceUpate = false) {}
    void updateSensors(bool forceUpate = false) {}

    void setWill() {}
};
HasClient *hasClient = new HasClient();

void hasTaskFunc(void *parameter) {
    while (true) {
        vTaskDelay(READ_DELAY);  // delay task xxx ms
    }
}
#endif