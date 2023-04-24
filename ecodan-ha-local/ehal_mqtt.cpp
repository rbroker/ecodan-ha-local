#include "ehal.h"
#include "ehal_config.h"
#include "ehal_diagnostics.h"
#include "ehal_hp.h"
#include "ehal_mqtt.h"
#include "ehal_thirdparty.h"

#include <WiFi.h>
#include <WiFiClient.h>

#include <chrono>
#include <cmath>
#include <thread>

namespace ehal::mqtt
{
    bool needsAutoDiscover = true;
    WiFiClient espClient;
    PubSubClient mqttClient(espClient);

    // https://arduinojson.org/v6/how-to/configure-the-serialization-of-floats/#how-to-reduce-the-number-of-decimal-places
    double round2(double value)
    {
        return (int)(value * 100 + 0.5) / 100.0;
    }

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
        tpl.replace(F("{{min_temp}}"), String(hp::get_min_thermostat_temperature()));
        tpl.replace(F("{{max_temp}}"), String(hp::get_max_thermostat_temperature()));
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

    String unique_entity_name(const String& name)
    {
        return name + "_" + device_mac();
    }

    void add_discovery_device_object(DynamicJsonDocument& doc)
    {
        JsonObject device = doc.createNestedObject("device");
        JsonArray identifiers = device.createNestedArray("ids");
        identifiers.add(device_mac());

        device["name"] = "Mitsubishi A2W Heat Pump";
        device["sw"] = get_software_version();
        device["mdl"] = hp::get_device_model();
        device["mf"] = "MITSUBISHI ELECTRIC";
        device["cu"] = String("http://") + WiFi.localIP().toString() + "/configuration";
    }

    bool publish_mqtt(const String& topic, const String& payload, bool retain = false)
    {
        mqttClient.beginPublish(topic.c_str(), payload.length(), retain);
        mqttClient.print(payload);
        return mqttClient.endPublish();
    }

    bool publish_mqtt(const String& topic, const DynamicJsonDocument& json, bool retain = false)
    {
        String output;
        serializeJson(json, output);
        return publish_mqtt(topic, output, retain);
    }

    bool publish_ha_climate_auto_discover()
    {
        // https://www.home-assistant.io/integrations/climate.mqtt/
        String uniqueName = unique_entity_name("climate_control");

        const auto& config = config_instance();
        String discoveryTopic = String("homeassistant/climate/") + uniqueName + "/config";
        String stateTopic = config.MqttTopic + "/" + uniqueName + "/state";

        DynamicJsonDocument payloadJson(8192);
        payloadJson["name"] = uniqueName;
        payloadJson["unique_id"] = uniqueName;
        payloadJson["icon"] = "mdi:heat-pump-outline";

        add_discovery_device_object(payloadJson);

        payloadJson["mode_stat_t"] = stateTopic;
        payloadJson["mode_stat_tpl"] = get_mode_status_template();
        payloadJson["act_t"] = stateTopic;
        payloadJson["act_tpl"] = get_action_status_template();
        payloadJson["temp_stat_t"] = stateTopic;
        payloadJson["temp_stat_tpl"] = get_temperature_status_template();
        payloadJson["curr_temp_t"] = stateTopic;
        payloadJson["curr_temp_tpl"] = get_current_temperature_status_template();

        {
            auto& status = hp::get_status();
            std::lock_guard<hp::Status> lock{status};

            payloadJson["initial"] = status.Zone1SetTemperature;
            payloadJson["min_temp"] = hp::get_min_thermostat_temperature();
            payloadJson["max_temp"] = hp::get_max_thermostat_temperature();
            payloadJson["temp_unit"] = "C";
            payloadJson["temp_step"] = hp::get_temperature_step();
        }

        JsonArray modes = payloadJson.createNestedArray("modes");
        modes.add("heat");
        modes.add("off");

        if (!publish_mqtt(discoveryTopic, payloadJson, /* retain =*/true))
        {
            log_web("Failed to publish homeassistant climate entity auto-discover");
            return false;
        }

        return true;
    }

    bool publish_ha_binary_sensor_auto_discover(String name)
    {
        const auto& config = config_instance();
        String uniqueName = unique_entity_name(name);
        String discoveryTopic = String("homeassistant/binary_sensor/") + uniqueName + "/config";
        String stateTopic = config.MqttTopic + "/" + uniqueName + "/state";

        // https://www.home-assistant.io/integrations/binary_sensor.mqtt/
        DynamicJsonDocument payloadJson(4096);
        payloadJson["name"] = uniqueName;
        payloadJson["unique_id"] = uniqueName;

        add_discovery_device_object(payloadJson);

        payloadJson["stat_t"] = stateTopic;
        payloadJson["payload_off"] = "off";
        payloadJson["payload_on"] = "on";

        if (!publish_mqtt(discoveryTopic, payloadJson, /* retain =*/true))
        {
            log_web("Failed to publish homeassistant %s entity auto-discover", uniqueName.c_str());
            return false;
        }

        return true;
    }

    void publish_homeassistant_auto_discover()
    {
        if (!needsAutoDiscover)
            return;

        bool anyFailed = false;

        // https://www.home-assistant.io/integrations/mqtt/
        if (!publish_ha_climate_auto_discover())
            anyFailed = true;

        if (!publish_ha_binary_sensor_auto_discover("mode_defrost"))
            anyFailed = true;

        if (!publish_ha_binary_sensor_auto_discover("mode_dhw_boost"))
            anyFailed = true;

        if (!anyFailed)
        {
            needsAutoDiscover = false;
            log_web("Published homeassistant auto-discovery topics");
        }
    }

    void publish_climate_status()
    {
        DynamicJsonDocument json(1024);

        {
            auto& status = hp::get_status();
            std::lock_guard<hp::Status> lock{status};

            json["temperature"] = round2(status.Zone1SetTemperature);
            json["curr_temp"] = round2(status.Zone1RoomTemperature);
            json["mode"] = status.ha_mode_as_string();
            json["action"] = status.ha_action_as_string();
        }

        const auto& config = config_instance();
        String stateTopic = config.MqttTopic + "/" + unique_entity_name("climate_control") + "/state";
        if (!publish_mqtt(stateTopic, json))
            log_web("Failed to publish MQTT state for: %s", unique_entity_name("climate_control"));
    }

    void publish_binary_sensor_status(const String& name, bool on)
    {
        String state = on ? "on" : "off";
        const auto& config = config_instance();
        String stateTopic = config.MqttTopic + "/" + unique_entity_name(name) + "/state";
        if (!publish_mqtt(stateTopic, state))
            log_web("Failed to publish MQTT state for: %s", unique_entity_name(name));
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

        return true;
    }

    void handle_loop()
    {
        if (hp::is_connected())
        {
            if (periodic_update_tick())
            {
                // Re-establish MQTT connection if we need to.
                connect();

                // Publish homeassistant auto-discovery messages if we need to.
                publish_homeassistant_auto_discover();

                // Update all entity statuses.
                publish_climate_status();

                auto& status = hp::get_status();
                std::lock_guard<hp::Status> lock{status};
                publish_binary_sensor_status("mode_defrost", status.DefrostActive);
                publish_binary_sensor_status("mode_dhw_boost", status.DhwBoostActive);
            }
        }

        mqttClient.loop();
    }

    bool is_connected()
    {
        return mqttClient.connected();
    }
} // namespace ehal::mqtt