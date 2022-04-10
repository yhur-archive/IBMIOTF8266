# IBM IOT Foundation for ESP8266

With this library, the developer can create an ESP8266 IBM IOT device which
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
    if (!strcmp(updateTopic, topic)) {
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


## dependancy and tips
This library uses LittleFS instead of SPIFFS, and needs PubSubClient, ArduinoJson, ConfigPortal8266.

And if you need to send a long MQTT message, then you can increase the default size of 256 byte by setting the build time variable MQTT_MAX_PACKET_SIZE as below. You may need this, if you want to implement the IR remote, since some appliance would need a long sequence of control signal.

The following is a snnipet of a tested platformio.ini.

```
board_build.filesystem=littlefs 
build_flags = 
	-D MQTT_MAX_PACKET_SIZE=512
lib_deps = 
	knolleary/PubSubClient@^2.8
	bblanchon/ArduinoJson@^6.18.5
	https://github.com/iotlab101/ConfigPortal8266
	https://github.com/yhur/IBMIOTF8266
 ```

### More Info.
It handles

1. IBM IOT Device Management like remote boot, remote factory reset
2. IBM IOT meta data update
3. Over the Air Firmware Update
4. Configuration reporting
5. And this uses the https://github.com/yhur/ConfigPortal8266 as the WiFi configuration utility.
