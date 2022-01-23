// IBM IOT Device
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <PubSubClient.h>
#include <ESP8266httpUpdate.h>
#include <ConfigPortal8266.h>

char                publishTopic[200];
char                infoTopic[200];
char                commandTopic[200];
char                responseTopic[200];
char                manageTopic[200];
char                updateTopic[200];
char                rebootTopic[200];
char                resetTopic[200];

const char*         t_publishTopic  = "iot-2/type/%s/id/%s/evt/status/fmt/json";
const char*         t_infoTopic     = "iot-2/type/%s/id/%s/evt/info/fmt/json";
const char*         t_commandTopic  = "iot-2/type/%s/id/%s/cmd/+/fmt/+";
const char*         t_commandBase   = "iot-2/type/%s/id/%s/cmd/";
const char*         t_responseTopic = "iotdm-1/type/%s/id/%s/response";
const char*         t_manageTopic   = "iotdevice-1/type/%s/id/%s/mgmt/manage";
const char*         t_updateTopic   = "iotdm-1/type/%s/id/%s/device/update";
const char*         t_rebootTopic   = "iotdm-1/type/%s/id/%s/mgmt/initiate/device/reboot";
const char*         t_resetTopic    = "iotdm-1/type/%s/id/%s/mgmt/initiate/device/factory_reset";

ESP8266WebServer    server(80);
WiFiClient          espClient;
PubSubClient        client(espClient);
char                iot_server[100];
char                msgBuffer[JSON_BUFFER_LENGTH];
int                 cmdBaseLen = 0;
unsigned long       pubInterval;
unsigned long       lastWiFiConnect;

bool subscribeTopic(const char* topic) {
    if (client.subscribe(topic)) {
        Serial.printf("Subscription to %s OK\n", topic);
        return true;
    } else {
        Serial.printf("Subscription to %s Failed\n", topic);
        return false;
    }
}

void initDevice() {
    loadConfig();
    if (cfg.containsKey("devType") && cfg.containsKey("devType")) {
        char temp[200];
        sprintf(publishTopic, t_publishTopic, (const char*)cfg["devType"], (const char*)cfg["devId"]);
        sprintf(infoTopic, t_infoTopic, (const char*)cfg["devType"], (const char*)cfg["devId"]);
        sprintf(commandTopic, t_commandTopic, (const char*)cfg["devType"], (const char*)cfg["devId"]);
        sprintf(responseTopic, t_responseTopic, (const char*)cfg["devType"], (const char*)cfg["devId"]);
        sprintf(manageTopic, t_manageTopic, (const char*)cfg["devType"], (const char*)cfg["devId"]);
        sprintf(updateTopic, t_updateTopic, (const char*)cfg["devType"], (const char*)cfg["devId"]);
        sprintf(rebootTopic, t_rebootTopic, (const char*)cfg["devType"], (const char*)cfg["devId"]);
        sprintf(resetTopic, t_resetTopic, (const char*)cfg["devType"], (const char*)cfg["devId"]);
        sprintf(temp, t_commandBase, (const char*)cfg["devType"], (const char*)cfg["devId"]);
        cmdBaseLen = strlen(temp);
    }
}

void iot_connect() {

    while (!client.connected()) {
        sprintf(msgBuffer,"d:%s:%s", (const char*)cfg["devType"], (const char*)cfg["devId"]);
        if (client.connect(msgBuffer)) {
            Serial.println("MQ connected");
        } else {
            if( digitalRead(RESET_PIN) == 0 ) {
                reboot();
            }
            if(WiFi.status() == WL_CONNECTED) {
                lastWiFiConnect = millis();
                Serial.printf("MQ Connection fail RC = %d, try again in 5 seconds\n", client.state());
                delay(5000);
            } else {
                Serial.println("Reconnecting to WiFi");
                WiFi.begin();
                while (WiFi.status() != WL_CONNECTED) {
                    if(lastWiFiConnect + 3600000 < millis()) {
                        reboot();
                    } else {
                        delay(5000);
                        Serial.print("*");
                    }
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

    if (!strcmp(responseTopic, topic)) {        // strcmp return 0 if both string matches
        return;                                 // just print of response for now
    } else if (!strcmp(rebootTopic, topic)) {   // rebooting
        reboot();
    } else if (!strcmp(resetTopic, topic)) {    // clear the configuration and reboot
        reset_config();
        ESP.restart();
    } else if (!strcmp(updateTopic, topic)) {
        JsonArray fields = d["fields"];
        for(JsonArray::iterator it=fields.begin(); it!=fields.end(); ++it) {
            DynamicJsonDocument field = *it;
            const char* fieldName = field["field"];
            if (strcmp (fieldName, "metadata") == 0) {
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
    } else if (!strncmp(commandTopic, topic, cmdBaseLen)) {
        if (d.containsKey("upgrade")) {
            JsonObject upgrade = d["upgrade"];
            String response = "{\"OTA\":{\"status\":";
            if(upgrade.containsKey("server") && 
                        upgrade.containsKey("port") && 
                        upgrade.containsKey("uri")) {
	            const char *fw_server = upgrade["server"];
	            int fw_server_port = atoi(upgrade["port"]);
	            const char *fw_uri = upgrade["uri"];
                ESPhttpUpdate.onProgress(update_progress);
                ESPhttpUpdate.onError(update_error);
                client.publish(infoTopic,"{\"info\":{\"upgrade\":\"Device will be upgraded.\"}}" );
	            t_httpUpdate_return ret = ESPhttpUpdate.update(espClient, fw_server, fw_server_port, fw_uri);
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
            // if the config answer is not published to the MQTT, then 
            //     check MQTT_MAX_PACKET_SIZE in PubSubClient.h
            char maskBuffer[JSON_BUFFER_LENGTH];
            maskConfig(maskBuffer);
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
