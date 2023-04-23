#include "ehal.h"
#include "ehal_config.h"
#include "ehal_diagnostics.h"
#include "ehal_hp.h"
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
    String mqttStateTopic;
    String mqttAvailabilityTopic;  

    String get_mode_status_template()
    {
        String tpl(F(R"(
{% if (value_json is defined and value_json.mode is defined) %}
{{ value_json.mode }}
{% else %}
off
{% endif %})"));

        tpl.replace(F("\n"), "");
        tpl.trim();
        return tpl;
    }

    String get_action_status_template()
    {
        String tpl(F(R"(
{% if (value_json is defined and value_json.mode is defined) %}
{{ value_json.action }}
{% else %}
off
{% endif %})"));

        tpl.replace(F("\n"), "");
        tpl.trim();
        return tpl;
    }

    String get_temperature_status_template()
    {
        String tpl(F(R"(
{% if (value_json is defined and value_json.temperature is defined) %}
{% if (value_json.temperature|int >= {{min_temp}} and value_json.temperature|int <= {{max_temp}}) %}
{{ value_json.temperature }}
{% elif (value_json.temperature|int < {{min_temp}}) %}
{{min_temp}}
{% elif (value_json.temperature|int > {{max_temp}}) %}
{{max_temp}}
{% endif %}
{% else %}
21
{% endif %})"));

        auto& status = hp::get_status();
        std::lock_guard<hp::Status> lock{status};

        tpl.replace(F("\n"), "");
        tpl.replace(F("{{min_temp}}"), String(status.MinimumFlowTemperature));
        tpl.replace(F("{{max_temp}}"), String(status.MaximumFlowTemperature));
        tpl.trim();

        return tpl;
    }

    String get_current_temperature_status_template()
    {
        String tpl(F(R"(
{% if (value_json is defined and value_json.curr_temp is defined) %}
{{ value_json.curr_temp }}
{% else %}
0
{% endif %})"));

        tpl.replace(F("\n"), "");
        tpl.trim();

        return tpl;
    }

    void mqtt_callback(const char* topic, byte* payload, uint length)
    {
        log_web("Received MQTT topic: %s", topic);
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

        static auto last_attempt = std::chrono::steady_clock::now();

        auto now = std::chrono::steady_clock::now();
        if (now - last_attempt < std::chrono::seconds(10))
            return false;

        last_attempt = now;

        return true;
    }

    String entity_name()
    {
        return String("ecodan_hp_") + device_mac();
    }

    void publish_homeassistant_auto_discover()
    {
        // https://www.home-assistant.io/integrations/mqtt/
        // https://www.home-assistant.io/integrations/climate.mqtt/

        DynamicJsonDocument payloadJson(4096);
        payloadJson["name"] = entity_name();
        payloadJson["unique_id"] = device_mac();
        payloadJson["icon"] = "mdi:heat-pump-outline";

        JsonObject device = payloadJson.createNestedObject("device");
        JsonArray identifiers = device.createNestedArray("ids");
        identifiers.add(entity_name());

        device["name"] = entity_name();
        device["sw"] = get_software_version();
        device["mdl"] = hp::get_device_model();
        device["mf"] = "MITSUBISHI ELECTRIC";
        device["cu"] = String("http://") + WiFi.localIP().toString() + "/configuration";

        payloadJson["device_class"] = "climate";
        payloadJson["mode_stat_t"] = mqttStateTopic;
        payloadJson["mode_stat_tpl"] = get_mode_status_template();
        payloadJson["act_t"] = mqttStateTopic;
        payloadJson["act_tpl"] = get_action_status_template();
        payloadJson["temp_stat_t"] = mqttStateTopic;
        payloadJson["temp_stat_tpl"] = get_temperature_status_template();
        payloadJson["curr_temp_t"] = mqttStateTopic;
        payloadJson["curr_temp_tpl"] = get_current_temperature_status_template();
        payloadJson["avty_t"] = mqttAvailabilityTopic;
        payloadJson["pl_not_avail"] = "offline";
        payloadJson["pl_avail"] = "online";

        {
            auto& status = hp::get_status();
            std::lock_guard<hp::Status> lock{status};

            payloadJson["initial"] = status.Zone1SetTemperature;
            payloadJson["min_temp"] = status.MinimumFlowTemperature;
            payloadJson["max_temp"] = status.MaximumFlowTemperature;
            payloadJson["temp_unit"] = "C";
            payloadJson["temp_step"] = hp::get_temperature_step();
        }

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

    void publish_availability()
    {
        static String status;
        bool statusChanged = false;

        if (hp::is_connected())
        {
            if (status != "online")            
            {
                statusChanged = true;
                publish_homeassistant_auto_discover();            
            }

            status = F("online");
        }
        else
        {
            if (status != "offline")
                statusChanged = true;

            status = F("offline");
        }

        // Don't re-publish if nothing's changed since the last time we advertised the state.
        if (!statusChanged)
            return;

        if (mqttClient.publish(mqttAvailabilityTopic.c_str(), status.c_str(), true))
        {
            log_web("Published HP availability: %s", status);
        }
        else
        {
            // If we failed to publish the status, we'll need to re-publush availability when
            // the connection is recovered.
            status.clear();
        }
    }

    void publish_status()
    {
        DynamicJsonDocument json(1024);

        {
            auto& status = hp::get_status();
            std::lock_guard<hp::Status> lock{status};

            json["temperature"] = String(status.Zone1SetTemperature, 1);
            json["curr_temp"] = String(status.Zone1RoomTemperature, 1);
            json["mode"] = status.ha_mode_as_string();
            json["action"] = status.ha_action_as_string();
        }

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
        mqttClient.setKeepAlive(15);
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
        mqttDiscovery = String("homeassistant/climate/") + entity_name() + "/config";
        mqttStateTopic = config.MqttTopic + "/" + entity_name() + "/state";
        mqttAvailabilityTopic = config.MqttTopic + "/" + entity_name() + "/availability";

        return true;
    }

    void handle_loop()
    {
        if (periodic_update_tick())
        {
            connect(); // Re-establish MQTT connection if we need to.

            publish_availability();

            if (hp::is_connected())
                publish_status();
        }

        mqttClient.loop();
    }

    bool is_connected()
    {
        return mqttClient.connected();
    }
} // namespace ehal::mqtt