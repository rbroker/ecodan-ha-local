#include "ehal_config.h"
#include "ehal_diagnostics.h"
#include "ehal_mqtt.h"
#include "ehal_thirdparty.h"

#include <WiFi.h>
#include <WiFiClient.h>

#include <chrono>
#include <thread>

namespace ehal::mqtt
{
    WiFiClient espClient;
    PubSubClient mqttClient(espClient);
    String mqttDiscovery;
    String mqttModeSetTopic;
    String mqttStateTopic;
    String mqttAvailabilityTopic;

    void mqtt_callback(const char* topic, byte* payload, uint length)
    {
        log_web("Received MQTT topic: %s", topic);

        String t(topic);
    }

    const char* get_connection_error_string()
    {
        switch (mqttClient.state())
        {
        case MQTT_CONNECTION_TIMEOUT:
            return "MQTT_CONNECTION_TIMEOUT";
        case MQTT_CONNECTION_LOST:
            return "MQTT_CONNECTION_LOST";
        case MQTT_CONNECT_FAILED:
            return "MQTT_CONNECT_FAILED";
        case MQTT_DISCONNECTED:
            return "MQTT_DISCONNECTED";
        case MQTT_CONNECTED:
            return "MQTT_CONNECTED";
        case MQTT_CONNECT_BAD_PROTOCOL:
            return "MQTT_CONNECT_BAD_PROTOCOL";
        case MQTT_CONNECT_BAD_CLIENT_ID:
            return "MQTT_CONNECT_BAD_CLIENT_ID";
        case MQTT_CONNECT_UNAVAILABLE:
            return "MQTT_CONNECT_UNAVAILABLE";
        case MQTT_CONNECT_BAD_CREDENTIALS:
            return "MQTT_CONNECT_BAD_CREDENTIALS";
        case MQTT_CONNECT_UNAUTHORIZED:
            return "MQTT_CONNECT_UNAUTHORIZED";
        default:
            return "Unknown";
        }
    }

    bool is_configured()
    {
        Config& config = config_instance();
        if (config.MqttServer.isEmpty())
            return false;

        if (config.MqttTopic.isEmpty())
            return false;

        return true;
    }

    bool periodic_update_tick()
    {
        if (!is_configured())
            return false;

        static std::chrono::steady_clock::time_point last_attempt = std::chrono::steady_clock::now();

        std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
        if (now - last_attempt < std::chrono::seconds(10))
            return false;

        last_attempt = now;
        return true;
    }

    String device_mac()
    {
        char deviceMac[17] = {};
        snprintf(deviceMac, sizeof(deviceMac), "%llx", ESP.getEfuseMac());
        return deviceMac;
    }

    String name()
    {        
        return String("ecodan_hp_") + device_mac();        
    }

    void publish_homeassistant_auto_discover()
    {
        DynamicJsonDocument payloadJson(4096);
        payloadJson["name"] = name();
        payloadJson["uniqueId"] = device_mac();

        JsonArray modes = payloadJson.createNestedArray("modes");
        modes.add("heat");
        modes.add("off");

        payloadJson["device_class"] = "climate";
        payloadJson["mode_cmd_t"] = mqttModeSetTopic;
        payloadJson["mode_stat_t"] = mqttStateTopic;
        payloadJson["avty_t"] = mqttAvailabilityTopic;
        payloadJson["pl_not_avail"] = "offline";
        payloadJson["pl_avail"] = "online";
        payloadJson["temp_unit"] = "C";

        JsonObject device = payloadJson.createNestedObject("device");
        device["ids"] = name();
        device["name"] = name();
        device["sw"] = get_software_version();
        device["mdl"] = "Ecodan PUZ-WM60VAA";
        device["mf"] = "MITSUBISHI ELECTRIC";
        device["configuration_url"] = String("http://") + WiFi.localIP().toString() + "/configuration";

        String output;
        serializeJson(payloadJson, output);
        mqttClient.beginPublish(mqttDiscovery.c_str(), output.length(), true);
        mqttClient.print(output);
        mqttClient.endPublish();

        log_web("Published homeassistant auto-discovery topic");
    }

    void publish_status_available()
    {
        mqttClient.publish(mqttAvailabilityTopic.c_str(), "online", true);
        log_web("Published HP status: online");
    }

    void publish_status()
    {
        // Dummy data until I get the actual heat pump
        DynamicJsonDocument json(1024);
        json["roomTemperature"] = "21";
        json["temperature"] = "7";
        json["mode"] = "off";
        json["action"] = "off";

        String mqttOutput;
        serializeJson(json, mqttOutput);

        mqttClient.publish_P(mqttStateTopic.c_str(), mqttOutput.c_str(), false);
    }

    bool connect()
    {
        if (mqttClient.connected())
            return true;

        Config& config = config_instance();
        if (!config.MqttPassword.isEmpty() && !config.MqttUserName.isEmpty())
        {
            log_web("MQTT user '%s' has configured password, connecting with credentials...", config.MqttUserName.c_str());
            if (!mqttClient.connect(WiFi.localIP().toString().c_str(), config.MqttUserName.c_str(), config.MqttPassword.c_str()))
            {
                log_web("MQTT connection failure: '%s'", get_connection_error_string());
                return false;
            }
        }
        else
        {
            log_web("MQTT username/password not configured, connecting as anonymous user...");
            if (!mqttClient.connect(WiFi.localIP().toString().c_str()))
            {
                log_web("MQTT connection failure: '%s'", get_connection_error_string());
                return false;
            }
        }

        if (mqttClient.connected())
        {
            log_web("Successfully established MQTT client connection!");
            publish_homeassistant_auto_discover();
            publish_status_available();
        }
        return true;
    }

    bool initialize()
    {
        if (!is_configured())
        {
            log_web("Unable to initialize MQTT, server is not configured.");
            return false;
        }

        Config& config = config_instance();
        mqttClient.setServer(config.MqttServer.c_str(), config.MqttPort);
        mqttClient.setCallback(mqtt_callback);

        for (int i = 0; i < 5; ++i)
        {
            connect();
            if (!mqttClient.connected())
            {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            else
            {
                break;
            }
        }

        // https://www.home-assistant.io/integrations/mqtt/
        // <discovery_prefix>/<component>/<object_id>/
        mqttDiscovery = String("homeassistant/climate/") + name() + "/config";
        mqttModeSetTopic = config.MqttTopic + "/" + name() + "/mode/set";
        mqttStateTopic = config.MqttTopic + "/" + name() + "/state";
        mqttAvailabilityTopic = config.MqttTopic + "/" + name() + "/availability";

        mqttClient.subscribe(mqttModeSetTopic.c_str());

        return true;
    }

    void handle_loop()
    {
        if (periodic_update_tick())
        {
            connect();
            publish_status();
        }

        mqttClient.loop();
    }
} // namespace ehal::mqtt