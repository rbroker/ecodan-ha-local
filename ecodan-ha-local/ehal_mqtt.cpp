#include "ehal.h"
#include "ehal_config.h"
#include "ehal_diagnostics.h"
#include "ehal_mqtt.h"
#include "ehal_hp.h"
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

    void publish_homeassistant_auto_discover()
    {
        // https://www.home-assistant.io/integrations/mqtt/
        // https://www.home-assistant.io/integrations/climate.mqtt/

        DynamicJsonDocument payloadJson(4096);
        payloadJson["name"] = hp::entity_name();
        payloadJson["unique_id"] = device_mac();
        payloadJson["icon"] = "mdi:heat-pump-outline";

        JsonObject device = payloadJson.createNestedObject("device");
        JsonArray identifiers = device.createNestedArray("ids");
        identifiers.add(hp::entity_name());

        device["name"] = hp::entity_name();
        device["sw"] = get_software_version();
        device["mdl"] = hp::get_device_model();
        device["mf"] = "MITSUBISHI ELECTRIC";
        device["cu"] = String("http://") + WiFi.localIP().toString() + "/configuration";        

        payloadJson["device_class"] = "climate";
        payloadJson["mode_cmd_t"] = mqttModeSetTopic;
        payloadJson["mode_stat_t"] = mqttStateTopic;
        payloadJson["mode_stat_tpl"] = hp::get_mode_status_template();
        payloadJson["temp_stat_t"] = mqttStateTopic;
        payloadJson["temp_stat_tpl"] = hp::get_temperature_status_template();
        payloadJson["curr_temp_t"] = mqttStateTopic;
        payloadJson["curr_temp_tpl"] = hp::get_current_temperature_status_template();
        payloadJson["avty_t"] = mqttAvailabilityTopic;        
        payloadJson["pl_not_avail"] = "offline";
        payloadJson["pl_avail"] = "online";

        payloadJson["initial"] = hp::get_initial_temperature();
        payloadJson["min_temp"] = hp::get_min_temperature();
        payloadJson["max_temp"] = hp::get_max_temperature();
        payloadJson["temp_unit"] = "C";
        payloadJson["temp_step"] = hp::get_temperature_step();

        JsonArray modes = payloadJson.createNestedArray("modes");
        modes.add("heat");
        modes.add("off");

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

    void publish_status_unavailable()
    {
        mqttClient.publish(mqttAvailabilityTopic.c_str(), "offline", true);
        log_web("Published HP status: offline");
    }

    void publish_status()
    {        
        DynamicJsonDocument json(1024);        
        json["temperature"] = hp::get_initial_temperature();
        json["curr_temp"] = hp::get_current_temperature();
        json["mode"] = hp::get_current_mode();
        json["action"] = hp::get_current_action();

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
        log_web("Initializing MQTT...");

        if (!is_configured())
        {
            log_web("Unable to initialize MQTT, server is not configured.");
            return false;
        }

        Config& config = config_instance();
        mqttClient.setServer(config.MqttServer.c_str(), config.MqttPort);
        mqttClient.setCallback(mqtt_callback);        

        // (At least) the first attempt to connect MQTT with valid credentials always seems
        // to fail, so
        for (int i = 0; i < 5; ++i)
        {            
            if (!connect())
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
        mqttDiscovery = String("homeassistant/climate/") + hp::entity_name() + "/config";
        mqttModeSetTopic = config.MqttTopic + "/" + hp::entity_name() + "/mode/set";
        mqttStateTopic = config.MqttTopic + "/" + hp::entity_name() + "/state";
        mqttAvailabilityTopic = config.MqttTopic + "/" + hp::entity_name() + "/availability";

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