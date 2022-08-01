// IBM IOT Device
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <PubSubClient.h>
#include <ESP8266httpUpdate.h>
#include <ConfigPortal8266.h>

const char          compile_date[] = __DATE__ " " __TIME__;
char                publishTopic[200]   = "iot-2/evt/status/fmt/json";
char                infoTopic[200]      = "iot-2/evt/info/fmt/json";
char                commandTopic[200]   = "iot-2/cmd/+/fmt/+";
char                responseTopic[200]  = "iotdm-1/response";
char                manageTopic[200]    = "iotdevice-1/mgmt/manage";
char                updateTopic[200]    = "iotdm-1/device/update";
char                rebootTopic[200]    = "iotdm-1/mgmt/initiate/device/reboot";
char                resetTopic[200]     = "iotdm-1/mgmt/initiate/device/factory_reset";

String              user_config_html = ""
    "<p><input type='text' name='org' placeholder='org/edge'>"
    "<p><input type='text' name='devType' placeholder='Device Type'>"
    "<p><input type='text' name='devId' placeholder='Device Id'>"
    "<p><input type='text' name='token' placeholder='Device Token'>"
    "<p><input type='text' name='meta.pubInterval' placeholder='Publish Interval'>";

extern  String      user_html;

ESP8266WebServer    server(80);
WiFiClientSecure    wifiClientSecure;
WiFiClient          wifiClient;
PubSubClient        client;
char                iot_server[100];
char                msgBuffer[JSON_CHAR_LENGTH];
unsigned long       pubInterval;

char                fpFile[] = "/fingerprint.txt";
String              fingerprint = "B3 B7 C3 0D 9D 32 E6 A2 8A FC FD BA 11 BB 05 5E E1 D9 9E F7";
int                 mqttPort = 8883;

bool subscribeTopic(const char* topic) {
    if (client.subscribe(topic)) {
        Serial.printf("Subscription to %s OK\n", topic);
        return true;
    } else {
        Serial.printf("Subscription to %s Failed\n", topic);
        return false;
    }
}

void toGatewayTopic(char* topic, const char* devType, const char* devId) {
    char buffer[200];
    char devInfo[100];
    sprintf(devInfo, "/type/%s/id/%s", devType, devId);

    char* slash = strchr(topic, '/');
    int len = slash - topic;
    strncpy(buffer, topic, len);
    strcpy(buffer + len, devInfo);
    strcpy(buffer + strlen(buffer), slash);
    strcpy(topic, buffer);
}

void initDevice() {
    user_config_html += user_html;
    loadConfig();

    if(!cfg.containsKey("config") || strcmp((const char*)cfg["config"], "done")) {
        configDevice();
        // the device will be configured and rebooted in the configDevice()
    }

    String org = cfg["org"];
    if (org.indexOf(".") == -1) {
        if (LittleFS.exists(fpFile)) {
            File f = LittleFS.open(fpFile, "r");
            fingerprint = f.readString();
            fingerprint.trim();
            f.close();
        }
        wifiClientSecure.setFingerprint(fingerprint.c_str());
        client.setClient(wifiClientSecure);
        sprintf(iot_server, "%s.messaging.internetofthings.ibmcloud.com", (const char*)cfg["org"]);
    } else {
        const char* devType = (const char*)cfg["devType"];
        const char* devId = (const char*)cfg["devId"];
        toGatewayTopic(publishTopic, devType, devId);
        toGatewayTopic(infoTopic, devType, devId);
        toGatewayTopic(commandTopic, devType, devId);
        toGatewayTopic(responseTopic, devType, devId);
        toGatewayTopic(manageTopic, devType, devId);
        toGatewayTopic(updateTopic, devType, devId);
        toGatewayTopic(rebootTopic, devType, devId);
        toGatewayTopic(resetTopic, devType, devId);

        client.setClient(wifiClient);
        sprintf(iot_server, "%s", (const char*)cfg["org"]);
        mqttPort = 1883;
    }
}

void iot_connect() {

    while (!client.connected()) {
        int mqConnected = 0;
        if(mqttPort == 8883) {
            sprintf(msgBuffer,"d:%s:%s:%s", (const char*)cfg["org"], (const char*)cfg["devType"], (const char*)cfg["devId"]);
            mqConnected = client.connect(msgBuffer,"use-token-auth",cfg["token"]);
        } else {
            sprintf(msgBuffer,"d:%s:%s", (const char*)cfg["devType"], (const char*)cfg["devId"]);
            mqConnected = client.connect(msgBuffer);
        }
        if (mqConnected) {
            Serial.println("MQ connected");
        } else {
            if( digitalRead(RESET_PIN) == 0 ) {
                reboot();
            }
            if(WiFi.status() == WL_CONNECTED) {
                if(client.state() == -2) {
                    if (mqttPort == 8883) {
                        wifiClientSecure.connect(iot_server, mqttPort);
                    } else {
                        wifiClient.connect(iot_server, mqttPort);
                    }
                } else {
                    Serial.printf("MQ Connection fail RC = %d, try again in 5 seconds\n", client.state());
                }
                delay(5000);
            } else {
                Serial.println("Reconnecting to WiFi");
                WiFi.disconnect();
                WiFi.begin();
                int i = 0;
                while (WiFi.status() != WL_CONNECTED) {
                    Serial.print("*");
                    delay(5000);
                    if(i++ > 10) reboot();
                }
            }
        }
    }
    if (!subscribeTopic(responseTopic)) return;
    if (!subscribeTopic(rebootTopic)) return;
    if (!subscribeTopic(resetTopic)) return;
    if (!subscribeTopic(updateTopic)) return;
    if (!subscribeTopic(commandTopic)) return;
    JsonObject meta = cfg["meta"];
    StaticJsonDocument<512> root;
    JsonObject d = root.createNestedObject("d");
    JsonObject metadata = d.createNestedObject("metadata");
    for (JsonObject::iterator it=meta.begin(); it!=meta.end(); ++it) {
        metadata[it->key().c_str()] = it->value();
    }
    JsonObject supports = d.createNestedObject("supports");
    supports["deviceActions"] = true;
    serializeJson(root, msgBuffer);
    Serial.printf("publishing device metadata: %s\n", msgBuffer);
    if (client.publish(manageTopic, msgBuffer)) {
        serializeJson(d, msgBuffer);
        String info = String("{\"info\":") + String(msgBuffer) + String("}");
        client.publish(infoTopic, info.c_str());
    }
}

void set_iot_server() {
    if(mqttPort == 8883) {
        if (!wifiClientSecure.connect(iot_server, mqttPort)) {
            Serial.println("ssl connection failed");
            return;
        }
    } else {
        if (!wifiClient.connect(iot_server, mqttPort)) {
            Serial.println("connection failed");
            return;
        }
    }
    client.setServer(iot_server, mqttPort);   //IOT
    iot_connect();
}

void publishError(char *msg) {
    String payload = "{\"info\":{\"error\":";
    payload += "\"" + String(msg) + "\"}}";
    client.publish(infoTopic, (char*) payload.c_str());
    Serial.println(payload);
}

void update_progress(int cur, int total) {
    Serial.printf("CALLBACK:  HTTP update process at %d of %d bytes...\n", cur, total);
}

void update_error(int err) {
    Serial.printf("CALLBACK:  HTTP update fatal error code %d\n", err);
}

void handleIOTCommand(char* topic, JsonDocument* root) {
    JsonObject d = (*root)["d"];

    if (strstr(topic, "/response")) {
        return;                                 // just print of response for now
    } else if (strstr(topic, "/device/reboot")) {   // rebooting
        reboot();
    } else if (strstr(topic, "/device/factory_reset")) {    // clear the configuration and reboot
        reset_config();
        ESP.restart();
    } else if (strstr(topic, "/device/update")) {
        JsonArray fields = d["fields"];
        for(JsonArray::iterator it=fields.begin(); it!=fields.end(); ++it) {
            DynamicJsonDocument field = *it;
            const char* fieldName = field["field"];
            if (strstr(fieldName, "metadata")) {
                JsonObject fieldValue = field["value"];
                cfg.remove("meta");
                JsonObject meta = cfg.createNestedObject("meta");
                for (JsonObject::iterator fv=fieldValue.begin(); fv!=fieldValue.end(); ++fv) {
                    meta[(char*)fv->key().c_str()] = fv->value();
                }
                save_config_json();
            }
        }
        pubInterval = cfg["meta"]["pubInterval"];
    } else if (strstr(topic, "/cmd/")) {
        if (d.containsKey("upgrade")) {
            JsonObject upgrade = d["upgrade"];
            String response = "{\"OTA\":{\"status\":";
            if(upgrade.containsKey("server") && 
                        upgrade.containsKey("port") && 
                        upgrade.containsKey("uri")) {
		        Serial.println("firmware upgrading");
	            const char *fw_server = upgrade["server"];
	            int fw_server_port = atoi(upgrade["port"]);
	            const char *fw_uri = upgrade["uri"];
                ESPhttpUpdate.onProgress(update_progress);
                ESPhttpUpdate.onError(update_error);
                client.publish(infoTopic,"{\"info\":{\"upgrade\":\"Device will be upgraded.\"}}" );
	            t_httpUpdate_return ret = ESPhttpUpdate.update(wifiClient, fw_server, fw_server_port, fw_uri);
	            switch(ret) {
		            case HTTP_UPDATE_FAILED:
                        response += "\"[update] Update failed. http://" + String(fw_server);
                        response += ":"+ String(fw_server_port) + String(fw_uri) +"\"}}";
                        client.publish(infoTopic, (char*) response.c_str());
                        Serial.println(response);
		                break;
		            case HTTP_UPDATE_NO_UPDATES:
                        response += "\"[update] Update no Update.\"}}";
                        client.publish(infoTopic, (char*) response.c_str());
                        Serial.println(response);
		                break;
		            case HTTP_UPDATE_OK:
		                Serial.println("[update] Update ok."); // may not called we reboot the ESP
		                break;
	            }
            } else {
                response += "\"OTA Information Error\"}}";
                client.publish(infoTopic, (char*) response.c_str());
                Serial.println(response);
            }
        } else if (d.containsKey("config")) {
            char maskBuffer[JSON_CHAR_LENGTH];
            cfg["compile_date"] = compile_date;
            maskConfig(maskBuffer);
            cfg.remove("compile_date");
            String info = String("{\"config\":") + String(maskBuffer) + String("}");
            client.publish(infoTopic, info.c_str());
        }
    }
}
/* FW Upgrade informaiton 
 * var evt1 = { 'd': { 
 *   'upgrade' : {
 *       'server':'192.168.0.9',
 *       'port':'3000',
 *       'uri' : '/file/IOTPurifier4GW.ino.nodemcu.bin'
 *       }
 *   }
 * };
*/
