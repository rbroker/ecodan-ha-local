#include <WebServer.h>
#include <DNSServer.h>
#include <pgmspace.h>
#include <WiFi.h>
#include <Preferences.h>

#include <chrono>
#include <memory>
#include <mutex>
#include <thread>

#include <ArduinoJson.h> // ArduinoJson v6.21.2
#include <RingBuf.h> // RingBuffer v1.0.3

#include "ehal_config.h"
#include "ehal_page.h"

WebServer server(80);
ehal::Config config;

std::mutex diagnosticRingbufferLock;
RingBuf<String, 64> diagnosticRingbuffer;
std::unique_ptr<DNSServer> dnsServer;

void log(String message)
{     
    const uint8_t MAX_MESSAGE_LENGTH = 255U;

    // If diagnostic message exceeds maximum length, truncate it.
    if (message.length() > MAX_MESSAGE_LENGTH)
    {
        message = message.substring(0, MAX_MESSAGE_LENGTH);
    }

    std::lock_guard<std::mutex> lock(diagnosticRingbufferLock);
    diagnosticRingbuffer.push(message);
}

bool load_saved_configuration()
{
    Preferences prefs;
    prefs.begin("config", true);
    
    config.FirstTimeConfig = prefs.getBool("ftc", true);
    config.HostName = prefs.getString("hostname", "ecodan_ha_local");
    config.WifiSsid = prefs.getString("wifi_ssid");
    config.WifiPassword = prefs.getString("wifi_pw");
    config.MqttServer = prefs.getString("mqtt_server");
    config.MqttPort = prefs.getUShort("mqtt_port", 1883U);
    config.MqttUserName = prefs.getString("mqtt_username");
    config.MqttPassword = prefs.getString("mqtt_pw");
    config.MqttTopic = prefs.getString("mqtt_topic", "ecodan_hp");

    prefs.end();

    return true;
}

bool initialize_wifi_access_point()
{
    log("Initializing WiFi connection...");
    
    if (requires_first_time_configuration())
    {        
        if (!WiFi.softAP(config.HostName.c_str(), config.WifiPassword.c_str()))
        {
            log("Unable to create WiFi Access point!");            
            return false;
        }     
    }
    else
    {
        if (!WiFi.begin(config.WifiSsid.c_str(), config.WifiPassword.c_str()))
        {
            log("Unable to create WiFi STA mode connection!");
            return false;
        }        
    }

    while (WiFi.isConnected())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }    

    WiFi.setAutoReconnect(true);

    log("WiFi connection successfully initialized");   
    return true;
}

void http_configure()
{
    String page { FPSTR(ehal::PAGE_TEMPLATE) };
    page.replace(F("{{PAGE_SCRIPT}}"), FPSTR(ehal::SCRIPT_INJECT_AP_SSIDS));
    page.replace(F("{{PAGE_BODY}}"), FPSTR(ehal::BODY_TEMPLATE_CONFIG));
    page.replace(F("{{PAGE_TITLE}}"), F("Ecodan: Homeassistant Bridge"));

    page.replace(F("{{wifi_ssid}}"), config.WifiSsid);
    page.replace(F("{{wifi_pw}}"), config.WifiPassword);
    page.replace(F("{{mqtt_server}}"), config.MqttServer);
    page.replace(F("{{mqtt_port}}"), String(config.MqttPort));
    page.replace(F("{{mqtt_user}}"), config.MqttUserName);
    page.replace(F("{{mqtt_pw}}"), config.MqttPassword);
    page.replace(F("{{mqtt_topic}}"), config.MqttTopic);

    server.send(200, F("text/html"), page);
}

void http_save_configuration()
{   
    String page { FPSTR(ehal::PAGE_TEMPLATE) };    
    page.replace(F("{{PAGE_BODY}}"), FPSTR(ehal::BODY_TEMPLATE_CONFIG_SAVED));
    server.send(200, F("text/html"), page);

    Preferences prefs;
    prefs.begin("config", /* readonly = */false);
        
    prefs.putString("wifi_ssid", server.arg("wifi_ssid"));
    prefs.putString("wifi_pw", server.arg("wifi_pw"));
    prefs.putString("mqtt_server", server.arg("mqtt_server"));
    prefs.putUShort("mqtt_port", server.arg("mqtt_port").toInt());
    prefs.putString("mqtt_username", server.arg("mqtt_username"));
    prefs.putString("mqtt_pw", server.arg("mqtt_pw"));
    prefs.putString("mqtt_topic", server.arg("mqtt_topic"));
    prefs.putBool("ftc", false);

    prefs.end();

    std::this_thread::sleep_for(std::chrono::seconds(1));
    ESP.restart();
}

void http_clear_config()
{
    String page { FPSTR(ehal::PAGE_TEMPLATE) };    
    page.replace(F("{{PAGE_BODY}}"), FPSTR(ehal::BODY_TEMPLATE_CONFIG_CLEARED));
    server.send(200, F("text/html"), page);

    Preferences prefs;
    prefs.begin("config", /* readonly = */false);
    prefs.clear();
    prefs.end();

    std::this_thread::sleep_for(std::chrono::seconds(1));
    ESP.restart();
}

void http_root()
{
    http_configure();
}

void http_query_ssid_list()
{
    DynamicJsonDocument json(1024);
    JsonArray ssids = json.createNestedArray("ssids");

    int count = WiFi.scanNetworks();
    for (int i = 0; i < count; ++i)
    {
        ssids.add(WiFi.SSID(i));
    }

    String jsonOut;
    serializeJson(json, jsonOut);

    WiFi.scanDelete();

    server.send(200, F("text/plain"), jsonOut);
}

bool requires_first_time_configuration()
{
    return config.FirstTimeConfig;
}

bool initialize_captive_portal()
{    
    dnsServer.reset(new DNSServer());
    if (!dnsServer)
    {
        log("Failed to allocate DNS server!");
        return false;
    }

    const uint16_t DNS_PORT = 53;
    if (!dnsServer->start(DNS_PORT, "*", WiFi.softAPIP()))
    {
        log("Failed to start DNS server");
        return false;
    }

    server.on("/", http_configure);
    server.on("/save", http_save_configuration);  
    server.on("/query_ssid", http_query_ssid_list);
    server.on("/clear_config", http_clear_config);
    server.onNotFound(http_configure);
    return true;
}

void setup() 
{
    if (!load_saved_configuration())
    {
        log("Failed to load configuration!");
    }
    
    initialize_wifi_access_point();

    if (requires_first_time_configuration())
    {
        initialize_captive_portal();
    }
    else
    {
        server.on("/", http_configure);
        server.on("/save", http_save_configuration);  
        server.on("/query_ssid", http_query_ssid_list);
        server.on("/clear_config", http_clear_config);
        server.onNotFound(http_configure);
    }

    server.begin();
}

void loop() 
{
    if (dnsServer && requires_first_time_configuration())
    {
        dnsServer->processNextRequest();
    }

    server.handleClient();
    delay(25);
}
