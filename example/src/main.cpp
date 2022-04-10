#include <Arduino.h>
#include <IBMIOTF8266.h>

String user_html = ""
// USER CODE EXAMPLE : your custom config variable 
// in meta.XXXXX, XXXXX should match to ArduinoJson index to access
    "<p><input type='text' name='meta.yourVar' placeholder='Your Custom Config'>";
                    ;
// for meta.XXXXX, this var is the C variable to hold the XXXXX
int             customVar1;
// USER CODE EXAMPLE : your custom config variable

char*               ssid_pfix = (char*)"IOTValve";
unsigned long       lastPublishMillis = - pubInterval;
const int           RELAY = 15;

void publishData() {
    StaticJsonDocument<512> root;
    JsonObject data = root.createNestedObject("d");

// USER CODE EXAMPLE : command handling
    data["valve"] = digitalRead(RELAY) == 1 ? "on" : "off";
// USER CODE EXAMPLE : command handling

    serializeJson(root, msgBuffer);
    client.publish(publishTopic, msgBuffer);
}

void handleUserCommand(JsonDocument* root) {
    JsonObject d = (*root)["d"];
    
// USER CODE EXAMPLE : status/change update
// code if any of device status changes to notify the change
    if(d.containsKey("valve")) {
        if (strcmp(d["valve"], "on")) {
            digitalWrite(RELAY, LOW);
        } else {
            digitalWrite(RELAY, HIGH);
        }
        lastPublishMillis = - pubInterval;
    }
// USER CODE EXAMPLE
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
    if (!strncmp(updateTopic, topic, cmdBaseLen)) {
// USER CODE EXAMPLE : meta data update
// If any meta data updated on the Internet, it can be stored to local variable to use for the logic
// in cfg["meta"]["XXXXX"], XXXXX should match to one in the user_html
        customVar1 = cfg["meta"]["yourVar"];
// USER CODE EXAMPLE
    } else if (!strncmp(commandTopic, topic, cmdBaseLen)) {            // strcmp return 0 if both string matches
        handleUserCommand(&root);
    }
}

void setup() {
    Serial.begin(115200);
// USER CODE EXAMPLE : meta data update
    pinMode(RELAY, OUTPUT);
// USER CODE EXAMPLE

    initDevice();
    // If not configured it'll be configured and rebooted in the initDevice(),
    // If configured, initDevice will set the proper setting to cfg variable

    WiFi.mode(WIFI_STA);
    WiFi.begin((const char*)cfg["ssid"], (const char*)cfg["w_pw"]);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    // main setup
    Serial.printf("\nIP address : "); Serial.println(WiFi.localIP());
// USER CODE EXAMPLE : meta data to local variable
    JsonObject meta = cfg["meta"];
    pubInterval = meta.containsKey("pubInterval") ? atoi((const char*)meta["pubInterval"]) : 0;
    lastPublishMillis = - pubInterval;
// USER CODE EXAMPLE
    
    set_iot_server();
    client.setCallback(message);
    iot_connect();
}

void loop() {
    if (!client.connected()) {
        iot_connect();
    }
// USER CODE EXAMPLE : main loop
//     you can put any main code here, for example, 
//     the continous data acquistion and local intelligence can be placed here
// USER CODE EXAMPLE
    client.loop();
    if ((pubInterval != 0) && (millis() - lastPublishMillis > pubInterval)) {
        publishData();
        lastPublishMillis = millis();
    }
}