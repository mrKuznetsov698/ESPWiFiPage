#include <Arduino.h>
#include "ESP8266WiFi.h"
#include "ESP8266WebServer.h"
#include "DNSServer.h"
#include "FS.h"
#include "EEPROM.h"
#include "EEManager.h"

#define SETTINGS_MODE 0
#define HOTSPOT_MODE 1
#define WIFI_MODE 2

ESP8266WebServer server(80);
DNSServer dnsServer;

struct WiFiConf{
    char ssid[32];
    char pass[32];
    byte mode;
} WiFiConfig;

EEManager memory(WiFiConfig);

void prepare_struct();

void servers_begin();
void access_point_begin(const char*, const char*);
void wifi_connect();

void handle_index_page();
void handle_connect();
void handle_hotspot();
void handle_404();
char* read_file(const char* file);

void setup() {
    Serial.begin(115200);
    SPIFFS.begin();
    EEPROM.begin(128);
    prepare_struct();
    byte code = memory.begin(0, 'w');
    Serial.println();
    Serial.println(code == 1 ? "Read data" : "First start");
    switch (code) {
        case 1:
            switch (WiFiConfig.mode) {
                case WIFI_MODE:
                    Serial.println("Connect to wifi");
                    Serial.println(WiFiConfig.ssid);
                    Serial.println(WiFiConfig.pass);
                    wifi_connect();
                    servers_begin();
                    break;
                case HOTSPOT_MODE:
                    Serial.println("Access point mode");
                    Serial.println(WiFiConfig.ssid);
                    Serial.println(WiFiConfig.pass);
                    access_point_begin(WiFiConfig.ssid, WiFiConfig.pass);
                    servers_begin();
                    break;
                case SETTINGS_MODE:
                    Serial.println("Setting mode");
                    access_point_begin("Wemos", "");
                    servers_begin();
                    break;
            }
            break;
        case 2:
            Serial.println("Settings mode");
            access_point_begin("Wemos", "");
            servers_begin();
            break;
    }
}

void loop(){
    dnsServer.processNextRequest();
    server.handleClient();
}

// *****************************Handlers*****************************

void handle_reconfig(){
    WiFiConfig.mode = SETTINGS_MODE;
    memory.updateNow();
    ESP.restart();
}

void handle_index_page(){
    char* content = read_file(WiFiConfig.mode == SETTINGS_MODE ? "/config.html" : "/index.html");
    server.send(200, "text/html", content);
    free((void*)content);
}

void handle_connect(){
    char* ssid = (char*)server.arg("ssid").c_str();
    char* pass = (char*)server.arg("pass").c_str();
    WiFiConfig.mode = WIFI_MODE;
    WiFiConfig.ssid[0] = '\0';
    WiFiConfig.pass[0] = '\0';
    strcat(WiFiConfig.ssid, ssid);
    strcat(WiFiConfig.pass, pass);
    memory.updateNow();
    ESP.restart();
}

void handle_hotspot(){
    char* ssid = (char*)server.arg("ssid").c_str();
    char* pass = (char*)server.arg("pass").c_str();
    WiFiConfig.mode = HOTSPOT_MODE;
    WiFiConfig.ssid[0] = '\0';
    WiFiConfig.pass[0] = '\0';
    strcat(WiFiConfig.ssid, ssid);
    strcat(WiFiConfig.pass, pass);
    memory.updateNow();
    ESP.restart();
}

void handle_404(){
    char* content = read_file("/error.html");
    server.send(200, "text/html", content);
    free((void*)content);
}

// *****************************Tools*****************************
char* read_file(const char* file){
    File fl = SPIFFS.open(file, "r");
    char* content = (char*)malloc(4096);
    content[0] = '\0';
    if (!fl || fl.isDirectory())
        strcat(content, "Internal server error");
    else {
        unsigned int count = fl.readBytesUntil(EOF, content, 4096);
        content[count] = '\0';
    }
    return content;
}

// *****************************System functions*****************************

void servers_begin(){
    server.on("/", HTTP_GET, handle_index_page);
    server.on("/connect", HTTP_POST, handle_connect);
    server.on("/ap", HTTP_POST, handle_hotspot);
    server.on("/reconf", HTTP_GET, handle_reconfig);
    server.onNotFound(handle_404);

    server.begin();
}

void access_point_begin(const char* ssid, const char* pass){
    WiFi.mode(WIFI_AP);

    IPAddress appip  = IPAddress(192,168,1,1);
    IPAddress subnet = IPAddress(255,255,255,0);

    WiFi.softAPConfig(appip, appip, subnet);
    WiFi.softAP(ssid, pass);

    dnsServer.start(53, "esp-server.com", appip);
}

void wifi_connect(){
    WiFi.mode(WIFI_STA);

    WiFi.begin(WiFiConfig.ssid, WiFiConfig.pass);
    uint32_t begin = millis();
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print('.');
        if (millis() - begin > 30000){
            // Не смогли подключиться
            WiFiConfig.mode = SETTINGS_MODE;
            memory.updateNow();
            ESP.restart();
        }
        delay(500);
    }
}

void prepare_struct(){
    WiFiConfig.mode = SETTINGS_MODE;
    WiFiConfig.ssid[0] = '\0';
    WiFiConfig.pass[0] = '\0';
}