#ifndef ESPRINKLER_ESPRINKLER_H
#define ESPRINKLER_ESPRINKLER_H

#define RELAY_ON LOW
#define RELAY_OFF HIGH

typedef struct {
    const char *ssid;
    const char *pwd;
} ap_t;

void wifiConnect();
void initSerial();
void initGpio();
void initOTA();
void initWiFi();
void initServer();
void initMQTT();
void mqttPublish(const char *payload, const char *topic);
void mqttConnect();
void startSprinklers(const char *pins, uint32_t duration);
void stopSprinklers();
void refreshStatus();
String prettyBytes(uint32_t bytes);
uint32_t getUptimeSecs();

#endif //ESPRINKLER_ESPRINKLER_H
