# IBM IOT Foundation for ESP8266

With this library, the developer can create a WiFi ESP8266 device which
1. helps configure with the Captive Portal if not configured as in the following picture,
2. or boots with the stored configuration if configured already,
3. connects to the WiFi/IBM IOT Foundation and run the loop function

![IOT Device Setup Captive Portal](https://user-images.githubusercontent.com/13171662/150662713-58af1cfc-be48-457b-828a-d9c1afe0c561.jpg)

# How to use the IBMIOTF8266
You can create a PlatformIO project with the example directory and modify the src/main.cpp for your purpose and build it.

## src/main.cpp 
The following code is the example to use the library. 
```c
#include <Arduino.h>
#include <IBMIOTF8266.h>

// USER CODE EXAMPLE : Publish Interval. The periodic update is normally recommended.
// And this can be a good example for the user code addition
String user_html = ""
    "<p><input type='text' name='meta.pubInterval' placeholder='Publish Interval'>";
// USER CODE EXAMPLE : command handling

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
    if (!strcmp(updateTopic, topic)) {
// USER CODE EXAMPLE : meta data update
// If any meta data updated on the Internet, it can be stored to local variable to use for the logic
        pubInterval = cfg["meta"]["pubInterval"];
// USER CODE EXAMPLE
    } else if (!strncmp(commandTopic, topic, 10)) {            // strcmp return 0 if both string matches
        handleUserCommand(&root);
    }
}

void setup() {
    Serial.begin(115200);
// USER CODE EXAMPLE : meta data update
    pinMode(RELAY, OUTPUT);
// USER CODE EXAMPLE

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
```

## customization
If additional configuration parameter is needed, you can modify the `user_config_html` for your variable, and handle the variable as shown below. The custom varialbe here is yourVar in the `user_html` and handling in setup code as below

In the global section.
```c
String user_html = ""
    "<p><input type='text' name='meta.pubInterval' placeholder='Publish Interval'>";
```

In the functions.
```c
    JsonObject meta = cfg["meta"];
    pubInterval = meta.containsKey("pubInterval") ? atoi((const char*)meta["pubInterval"]) : 0;
```

