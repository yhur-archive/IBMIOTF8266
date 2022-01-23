#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <IBMIOTDevice7.h>

String user_config_html = ""
    "<p><input type='text' name='org' placeholder='org'>"
    "<p><input type='text' name='devType' placeholder='Device Type'>"
    "<p><input type='text' name='devId' placeholder='Device Id'>"
    "<p><input type='text' name='token' placeholder='Device Token'>"
    "<p><input type='text' name='meta.pubInterval' placeholder='Publish Interval'>";

char*               ssid_pfix = (char*)"IOTValve";
unsigned long       lastPublishMillis = - pubInterval;
const int           RELAY = 15;

void publishData() {
    StaticJsonDocument<512> root;
    JsonObject data = root.createNestedObject("d");

    data["valve"] = digitalRead(RELAY) == 1 ? "on" : "off";

    serializeJson(root, msgBuffer);
    client.publish(publishTopic, msgBuffer);
}

void handleUserCommand(JsonDocument* root) {
    JsonObject d = (*root)["d"];
    // put code for the user command here, and put the following
    // code if any of device status changes to notify the change

    if(d.containsKey("valve")) {
        if (strcmp(d["valve"], "on")) {
            digitalWrite(RELAY, LOW);
        } else {
            digitalWrite(RELAY, HIGH);
        }
        lastPublishMillis = - pubInterval;
    }
}

void message(char* topic, byte* payload, unsigned int payloadLength) {
    byte2buff(msgBuffer, payload, payloadLength);
    StaticJsonDocument<512> root;
    DeserializationError error = deserializeJson(root, String(msgBuffer));
  
    if (error) {
        Serial.println("handleCommand: payload parse FAILED");
        return;
    }

    handleIOTCommand(topic, &root);
    if (!strcmp(updateTopic, topic)) {
        pubInterval = cfg["meta"]["pubInterval"];
    } else if (!strncmp(commandTopic, topic, 10)) {            // strcmp return 0 if both string matches
        handleUserCommand(&root);
    }
}

void setup() {
    Serial.begin(115200);
    pinMode(RELAY, OUTPUT);

    initDevice();
    // *** If no "config" is found or "config" is not "done", run configDevice ***
    if(!cfg.containsKey("config") || strcmp((const char*)cfg["config"], "done")) {
        configDevice();
    }
    WiFi.mode(WIFI_STA);
    WiFi.begin((const char*)cfg["ssid"], (const char*)cfg["w_pw"]);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    // main setup
    Serial.printf("\nIP address : "); Serial.println(WiFi.localIP());
    JsonObject meta = cfg["meta"];
    pubInterval = meta.containsKey("pubInterval") ? atoi((const char*)meta["pubInterval"]) : 0;
    lastPublishMillis = - pubInterval;
    
    sprintf(iot_server, "%s.messaging.internetofthings.ibmcloud.com", (const char*)cfg["org"]);
    if (!espClient.connect(iot_server, 8883)) {
        Serial.println("connection failed");
        return;
    }
    client.setServer(iot_server, 8883);   //IOT
    client.setCallback(message);
    iot_connect();
}

void loop() {
    if (!client.connected()) {
        iot_connect();
    }
    client.loop();
    if ((pubInterval != 0) && (millis() - lastPublishMillis > pubInterval)) {
        publishData();
        lastPublishMillis = millis();
    }
}