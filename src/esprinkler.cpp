/**
 * ESPrinkler
 *
 * @see https://github.com/esp8266/Arduino
 * @see https://github.com/me-no-dev/ESPAsyncWebServer
 */
#include <Arduino.h>
#include <Hash.h>
#include <ESP8266WiFiMulti.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <ESP8266mDNS.h>
#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>
#include "esprinkler.h"
#include "config.h"

//****************************************************************************

bool shouldRestart = false;
bool otaInProgress = false;
bool sprInProgress = false;

uint32_t sprLastMillis;
uint32_t sprDuration = 0;
uint32_t sprElapsed = 0;

char sprPins[sizeof(SPRINKLER_PINS) + 1] = "";
char status[120] = "";

ESP8266WiFiMulti wifiMulti;
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
AsyncWebServer server(80);
AsyncEventSource events("/events");

void setup() {

    // Fill sprPins with '0'
    sprintf(sprPins, "%0*d", sizeof(sprPins) - 1, 0);

    initSerial();
    initGpio();
    initOTA();
    initWiFi();
    initMQTT();
    initServer();
}

void loop() {

    if (shouldRestart) {
        delay(500);
        ESP.restart();
    }

    uint32_t now = millis();

    // "Send status" timer
    static uint32_t staLastMillis = 0;
    if (now - staLastMillis >= 1000) {
        refreshStatus();
        events.send(status, "status");
        staLastMillis = millis();
    }

    // "Sprinkle in progress" timer
    if (sprInProgress) {
        sprElapsed += now - sprLastMillis;
        sprLastMillis = now;
        if (sprElapsed >= sprDuration) {
            stopSprinklers();
        }
    }

    // Handle wifi connection
    if (wifiMulti.run() != WL_CONNECTED) {
        WiFi.disconnect();
        wifiConnect();
        return;
    }

    // Handle OTA
    if (!sprInProgress) {
        ArduinoOTA.handle();
        if (otaInProgress) {
            return;
        }
    }

    // Handle MQTT
    if (!mqttClient.connected()) {
        mqttConnect();
    }
    mqttClient.loop();
}


//****************************************************************************

void initSerial() {
    Serial.begin(115200);
    Serial.setDebugOutput(SERIAL_SET_DEBUG_OUTPUT);
    Serial.println("\n\n*******************");
    Serial.printf("ESPrinkler v%s\n\n", VERSION);
    Serial.printf("STA MAC:            %s\n", WiFi.macAddress().c_str());
    Serial.printf("AP MAC:             %s\n", WiFi.softAPmacAddress().c_str());
    Serial.printf("Chip ID:            %6X\n", ESP.getChipId());
    Serial.printf("Free space:         %s\n", prettyBytes(ESP.getFreeSketchSpace()).c_str());
    Serial.printf("Sketch size:        %s\n", prettyBytes(ESP.getSketchSize()).c_str());
    Serial.printf("Chip size:          %s\n", prettyBytes(ESP.getFlashChipRealSize()).c_str());
    Serial.printf("SDK version:        %s\n", ESP.getSdkVersion());
    Serial.println("*******************");
}

void initGpio() {
    digitalWrite(LED_PIN, LOW);
    pinMode(LED_PIN, OUTPUT);
    for (uint8_t i = 0; i < sizeof(SPRINKLER_PINS); i++) {
        digitalWrite(SPRINKLER_PINS[i], HIGH);
        pinMode(SPRINKLER_PINS[i], OUTPUT);
    }
}

void initOTA() {

    ArduinoOTA.setHostname(ESP_NAME);

    ArduinoOTA.onStart([]() {
        otaInProgress = true;
        Serial.println("Start updating...");
        events.send("start", "ota");
    });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        char msg[4];
        snprintf(msg, sizeof(msg), "%u", (progress / (total / 100)));
        Serial.println(msg);
        events.send(msg, "ota");
    });

    ArduinoOTA.onEnd([]() {
        Serial.println("\nend");
        events.send("end", "ota");
        otaInProgress = false;
    });

    ArduinoOTA.onError([](ota_error_t error) {
        String msg;
        if (error == OTA_AUTH_ERROR) msg = "auth failed";
        else if (error == OTA_BEGIN_ERROR) msg = "begin failed";
        else if (error == OTA_CONNECT_ERROR) msg = "connect failed";
        else if (error == OTA_RECEIVE_ERROR) msg = "receive failed";
        else if (error == OTA_END_ERROR) msg = "end failed";
        Serial.printf("Error: %s", msg.c_str());
        events.send(msg.c_str());
    });

    ArduinoOTA.begin();
}

void wifiConnect() {
    while (wifiMulti.run() != WL_CONNECTED) {
        delay(500);
    }
    Serial.printf("Connected to %s with IP %s\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
    MDNS.addService("http", "tcp", 80);
}

void initWiFi() {
    Serial.println("Connecting to WiFi...");
/*
    WiFi.disconnect();
    delay(500);
*/
    WiFi.hostname(ESP_NAME);
    WiFi.mode(WIFI_STA);
    for (uint8_t i = 0; i < sizeof(AP_LIST) / sizeof AP_LIST[0]; i++) {
        wifiMulti.addAP(AP_LIST[i].ssid, AP_LIST[i].pwd);
    }
    wifiConnect();
}

void mqttPublish(const char *payload, const char *topic) {
    mqttClient.publish(topic, payload);
}

void mqttConnect() {
    while (!mqttClient.connected()) {
        Serial.println("Attempting MQTT connection...");
        if (mqttClient.connect(ESP_NAME)) {
            Serial.printf("Connected to %s\n", MQTT_HOST);
            mqttPublish("{\"event\":\"connected\"}", MQTT_OUT_TOPIC);
        } else {
            Serial.print("failed, rc=");
            Serial.print(mqttClient.state());
            Serial.println(" try again in 5 seconds");
            delay(5000);
        }
    }
}

void initMQTT() {
    mqttClient.setServer(MQTT_HOST, MQTT_PORT);
    mqttConnect();
}

String prettyBytes(uint32_t bytes) {

    const char *suffixes[7] = {"B", "KB", "MB", "GB", "TB", "PB", "EB"};
    uint8_t s = 0;
    double count = bytes;

    while (count >= 1024 && s < 7) {
        s++;
        count /= 1024;
    }
    if (count - floor(count) == 0.0) {
        return String((int) count) + suffixes[s];
    } else {
        return String(round(count * 10.0) / 10.0, 1) + suffixes[s];
    };
}

void initServer() {

    SPIFFS.begin();

    events.onConnect([](AsyncEventSourceClient *client) {
        client->send("Hello!", NULL, millis(), 1000);
    });

    server.addHandler(&events);

    server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request) {
        refreshStatus();
        request->send(200, "text/json", String(status));
    });

    server.on("/start", HTTP_GET, [](AsyncWebServerRequest *request) {

        uint8_t nbSprinklers = sizeof(SPRINKLER_PINS);

        // Validate pins parameter

        if (!request->hasParam("pins")) {
            request->send(500, "text/plain", "Missing parameter: pins");
            return;
        }

        char pins[nbSprinklers + 1];
        AsyncWebParameter *p = request->getParam("pins");
        p->value().toCharArray(pins, sizeof(pins));

        if (strlen(pins) != nbSprinklers || strstr(pins, "1") == NULL) {
            request->send(500, "text/plain", "Invalid  parameter: pins");
            return;
        }
        for (uint8_t i = 0; i < nbSprinklers; i++) {
            if (pins[i] != '0' && pins[i] != '1') {
                request->send(500, "text/plain", "Invalid parameter: pins");
                return;
            }
        }

        // Validate duration parameter

        if (!request->hasParam("duration")) {
            request->send(500, "text/plain", "Missing parameter: duration");
            return;
        }

        p = request->getParam("duration");
        uint32_t duration = (uint32_t) p->value().toInt();

        if (duration <= 0 || duration > MAX_DURATION) {
            request->send(500, "text/plain", "Invalid parameter: duration");
            return;
        }

        startSprinklers(pins, duration);
        request->send(204);

    });

    server.on("/stop", HTTP_GET, [](AsyncWebServerRequest *request) {
        stopSprinklers();
        request->send(204);
    });

    server.on("/restart", HTTP_GET, [](AsyncWebServerRequest *request) {
        shouldRestart = true;
        AsyncWebServerResponse *response = request->beginResponse(303);
        response->addHeader("Location", "/");
        request->send(response);
    });

    server.on("/about", HTTP_GET, [](AsyncWebServerRequest *request) {

        AsyncResponseStream *response = request->beginResponseStream("text/json");
        DynamicJsonBuffer jsonBuffer;
        JsonObject &json = jsonBuffer.createObject();

        json["version"] = VERSION;
        json["sdkVersion"] = ESP.getSdkVersion();
        json["coreVersion"] = ESP.getCoreVersion();
        json["resetReason"] = ESP.getResetReason();
        json["ssid"] = WiFi.SSID();
        json["ip"] = WiFi.localIP().toString();
        json["staMac"] = WiFi.macAddress();
        json["apMac"] = WiFi.softAPmacAddress();
        json["chipId"] = String(ESP.getChipId(), HEX);
        json["chipSize"] = prettyBytes(ESP.getFlashChipRealSize());
        json["sketchSize"] = prettyBytes(ESP.getSketchSize());
        json["freeSpace"] = prettyBytes(ESP.getFreeSketchSpace());
        json.printTo(*response);

        request->send(response);
    });

    server.serveStatic("/terminal", SPIFFS, "/terminal.html").setLastModified(LAST_MODIFIED);

    server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html").setLastModified(LAST_MODIFIED);

    server.onNotFound([](AsyncWebServerRequest *request) {
        request->send(404);
    });

    server.begin();
}

uint32_t getUptimeSecs() {
    static uint32_t uptime = 0;
    static uint32_t previousMillis = 0;
    uint32_t now = millis();

    uptime += (now - previousMillis) / 1000UL;
    previousMillis = now;
    return uptime;
}

/**
 * Start sprinklers
 *
 * @param pins     Selected GPIO as bit string (eg. "0110")
 * @param duration Sprinkling duration in minutes
 */
void startSprinklers(const char *pins, uint32_t duration) {

    strcpy(sprPins, pins);
    sprDuration = duration * 1000;
    sprElapsed = 0;
    sprLastMillis = millis();

    for (uint8_t i = 0; i < sizeof(SPRINKLER_PINS); i++) {
        digitalWrite(SPRINKLER_PINS[i], (pins[i] == '1' ? RELAY_ON : RELAY_OFF));
    }
    digitalWrite(LED_PIN, HIGH);

    sprInProgress = true;

    refreshStatus();
    events.send(status, "status");
}

void stopSprinklers() {

    // Publish status
    refreshStatus();
    mqttPublish(status, MQTT_OUT_TOPIC);

    // Stop sprinkling
    for (uint8_t i = 0; i < sizeof(SPRINKLER_PINS); i++) {
        digitalWrite(SPRINKLER_PINS[i], RELAY_OFF);
    }
    digitalWrite(LED_PIN, LOW);

    // Reset globals
    sprInProgress = false;
    sprDuration = 0;
    sprElapsed = 0;
    sprintf(sprPins, "%0*d", sizeof(sprPins) - 1, 0);

    refreshStatus();
    events.send(status, "status");
}

void refreshStatus() {
    snprintf(
            status,
            sizeof(status),
            "{\"event\":\"status\",\"pins\":\"%s\",\"duration\":%d,\"elapsed\":%d,\"left\":%d,\"uptime\":%d,\"heap\":%d}",
            sprPins,
            sprDuration / 1000UL,
            sprElapsed / 1000UL,
            sprDuration / 1000UL - sprElapsed / 1000UL,
            getUptimeSecs(),
            ESP.getFreeHeap()
    );
}
