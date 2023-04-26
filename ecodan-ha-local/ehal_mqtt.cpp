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
#include <string>
#include <thread>

namespace ehal::mqtt
{
#define SENSOR_STATE_TIMEOUT (300) // If we update HP state once a minute, expiring HA states after 300s seems appropriate.

    void publish_climate_status();

    bool needsAutoDiscover = true;
    WiFiClient espClient;
    PubSubClient mqttClient(espClient);

    enum class SensorType
    {
        POWER,
        FREQUENCY,
        TEMPERATURE
    };

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

        tpl.replace("\n", "");
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

        tpl.replace("\n", "");
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

        tpl.replace("\n", "");
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

        tpl.replace("\n", "");
        tpl.trim();

        return tpl;
    }

    void on_z1_temperature_set_command(const String& payload)
    {
        if (payload.isEmpty())
        {
            return;
        }

        if (!hp::set_z1_target_temperature(payload.toFloat()))
        {
            log_web(F("Failed to set z1 target temperature!"));
        }
    }

    void on_mode_set_command(const String& payload)
    {
        if (!hp::set_mode(payload))
        {
            log_web(F("Failed to set operation mode!"));
        }
    }

    void mqtt_callback(const char* topic, byte* payload, uint length)
    {
        auto& config = config_instance();
        String uniqueName = unique_entity_name(F("climate_control"));
        String tempCmdTopic = config.MqttTopic + "/" + uniqueName + F("/temp_cmd");
        String modeCmdTopic = config.MqttTopic + "/" + uniqueName + F("/mode_cmd");
        std::string payloadStr{reinterpret_cast<const char*>(payload), length}; // Arduino string can't construct from ptr+size.

        log_web(F("MQTT topic received: %s: '%s'"), topic, payloadStr.c_str());
        if (tempCmdTopic == topic)
        {
            on_z1_temperature_set_command(payloadStr.c_str());
            publish_climate_status();
        }
        else if (modeCmdTopic == topic)
        {
            on_mode_set_command(payloadStr.c_str());
        }
    }

    const char* get_connection_error_string()
    {
        switch (mqttClient.state())
        {
        case MQTT_CONNECTION_TIMEOUT:
            return F("MQTT_CONNECTION_TIMEOUT");
        case MQTT_CONNECTION_LOST:
            return F("MQTT_CONNECTION_LOST");
        case MQTT_CONNECT_FAILED:
            return F("MQTT_CONNECT_FAILED");
        case MQTT_DISCONNECTED:
            return F("MQTT_DISCONNECTED");
        case MQTT_CONNECTED:
            return F("MQTT_CONNECTED");
        case MQTT_CONNECT_BAD_PROTOCOL:
            return F("MQTT_CONNECT_BAD_PROTOCOL");
        case MQTT_CONNECT_BAD_CLIENT_ID:
            return F("MQTT_CONNECT_BAD_CLIENT_ID");
        case MQTT_CONNECT_UNAVAILABLE:
            return F("MQTT_CONNECT_UNAVAILABLE");
        case MQTT_CONNECT_BAD_CREDENTIALS:
            return F("MQTT_CONNECT_BAD_CREDENTIALS");
        case MQTT_CONNECT_UNAUTHORIZED:
            return F("MQTT_CONNECT_UNAUTHORIZED");
        default:
            return F("Unknown");
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

        auto now = std::chrono::steady_clock::now();
        static auto last_attempt = now + std::chrono::seconds(35);

        if (now - last_attempt < std::chrono::seconds(30))
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
        JsonObject device = doc.createNestedObject(F("device"));
        JsonArray identifiers = device.createNestedArray(F("ids"));
        identifiers.add(device_mac());

        device[F("name")] = F("Mitsubishi A2W Heat Pump");
        device[F("sw")] = get_software_version();
        device[F("mdl")] = hp::get_device_model();
        device[F("mf")] = F("MITSUBISHI ELECTRIC");
        device[F("cu")] = FPSTR("http://") + WiFi.localIP().toString() + F("/configuration");
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
        String uniqueName = unique_entity_name(F("climate_control"));

        const auto& config = config_instance();
        String discoveryTopic = FPSTR("homeassistant/climate/") + uniqueName + F("/config");
        String stateTopic = config.MqttTopic + "/" + uniqueName + F("/state");
        String tempCmdTopic = config.MqttTopic + "/" + uniqueName + F("/temp_cmd");
        String modeCmdTopic = config.MqttTopic + "/" + uniqueName + F("/mode_cmd");

        DynamicJsonDocument payloadJson(8192);
        payloadJson[F("name")] = uniqueName;
        payloadJson[F("unique_id")] = uniqueName;
        payloadJson[F("icon")] = F("mdi:heat-pump-outline");

        add_discovery_device_object(payloadJson);

        payloadJson[F("mode_stat_t")] = stateTopic;
        payloadJson[F("mode_stat_tpl")] = get_mode_status_template();
        payloadJson[F("act_t")] = stateTopic;
        payloadJson[F("act_tpl")] = get_action_status_template();
        payloadJson[F("temp_stat_t")] = stateTopic;
        payloadJson[F("temp_stat_tpl")] = get_temperature_status_template();
        payloadJson[F("curr_temp_t")] = stateTopic;
        payloadJson[F("curr_temp_tpl")] = get_current_temperature_status_template();
        payloadJson[F("temp_cmd_t")] = tempCmdTopic;
        payloadJson[F("temp_cmd_tpl")] = F("{{ value|float }}");
        payloadJson[F("mode_cmt_t")] = modeCmdTopic;
        payloadJson[F("temp_cmd_tpl")] = F("{{ value }}");

        {
            auto& status = hp::get_status();
            std::lock_guard<hp::Status> lock{status};

            payloadJson[F("initial")] = status.Zone1SetTemperature;
            payloadJson[F("min_temp")] = hp::get_min_thermostat_temperature();
            payloadJson[F("max_temp")] = hp::get_max_thermostat_temperature();
            payloadJson[F("temp_unit")] = "C";
            payloadJson[F("temp_step")] = hp::get_temperature_step();
        }

        JsonArray modes = payloadJson.createNestedArray(F("modes"));
        modes.add(F("heat"));
        modes.add(F("off"));

        if (!publish_mqtt(discoveryTopic, payloadJson, /* retain =*/true))
        {
            log_web(F("Failed to publish homeassistant climate entity auto-discover"));
            return false;
        }

        return true;
    }

    bool publish_ha_binary_sensor_auto_discover(const String& name)
    {
        const auto& config = config_instance();
        String uniqueName = unique_entity_name(name);
        String discoveryTopic = FPSTR("homeassistant/binary_sensor/") + uniqueName + F("/config");
        String stateTopic = config.MqttTopic + "/" + uniqueName + F("/state");

        // https://www.home-assistant.io/integrations/binary_sensor.mqtt/
        DynamicJsonDocument payloadJson(4096);
        payloadJson[F("name")] = uniqueName;
        payloadJson[F("unique_id")] = uniqueName;

        add_discovery_device_object(payloadJson);

        payloadJson[F("stat_t")] = stateTopic;
        payloadJson[F("payload_off")] = F("off");
        payloadJson[F("payload_on")] = F("on");
        payloadJson[F("exp_aft")] = SENSOR_STATE_TIMEOUT;

        if (!publish_mqtt(discoveryTopic, payloadJson, /* retain =*/true))
        {
            log_web(F("Failed to publish homeassistant %s entity auto-discover"), uniqueName.c_str());
            return false;
        }

        return true;
    }

    bool publish_ha_float_sensor_auto_discover(const String& name, SensorType type)
    {
        const auto& config = config_instance();
        String uniqueName = unique_entity_name(name);
        String discoveryTopic = FPSTR("homeassistant/sensor/") + uniqueName + F("/config");
        String stateTopic = config.MqttTopic + "/" + uniqueName + F("/state");

        // https://www.home-assistant.io/integrations/sensor.mqtt/
        DynamicJsonDocument payloadJson(4096);
        payloadJson[F("name")] = uniqueName;
        payloadJson[F("unique_id")] = uniqueName;

        add_discovery_device_object(payloadJson);

        payloadJson[F("stat_t")] = stateTopic;
        payloadJson[F("val_tpl")] = F("{{ value|float }}");
        payloadJson[F("exp_aft")] = SENSOR_STATE_TIMEOUT;

        switch (type)
        {
        case SensorType::POWER:
            payloadJson[F("unit_of_meas")] = F("kWh");
            payloadJson[F("icon")] = F("mdi:lightning-bolt");
            payloadJson[F("stat_cla")] = F("total");
            payloadJson[F("dev_cla")] = F("energy");
            break;

        case SensorType::FREQUENCY:
            payloadJson[F("unit_of_meas")] = F("Hz");
            payloadJson[F("icon")] = F("mdi:fan");
            payloadJson[F("dev_cla")] = F("frequency");
            break;

        case SensorType::TEMPERATURE:
            payloadJson[F("unit_of_meas")] = F("°C");
            payloadJson[F("dev_cla")] = F("temperature");
            break;

        default:
            break;
        }

        if (!publish_mqtt(discoveryTopic, payloadJson, /* retain =*/true))
        {
            log_web(F("Failed to publish homeassistant %s entity auto-discover"), uniqueName.c_str());
            return false;
        }

        return true;
    }

    bool publish_ha_string_sensor_auto_discover(const String& name, const String& icon = "")
    {
        const auto& config = config_instance();
        String uniqueName = unique_entity_name(name);
        String discoveryTopic = FPSTR("homeassistant/sensor/") + uniqueName + F("/config");
        String stateTopic = config.MqttTopic + "/" + uniqueName + F("/state");

        // https://www.home-assistant.io/integrations/sensor.mqtt/
        DynamicJsonDocument payloadJson(4096);
        payloadJson[F("name")] = uniqueName;
        payloadJson[F("unique_id")] = uniqueName;

        add_discovery_device_object(payloadJson);

        payloadJson[F("stat_t")] = stateTopic;
        payloadJson[F("val_tpl")] = F("{{ value }}");
        payloadJson[F("exp_aft")] = SENSOR_STATE_TIMEOUT;

        if (!icon.isEmpty())
        {
            payloadJson["icon"] = icon;
        }

        if (!publish_mqtt(discoveryTopic, payloadJson, /* retain =*/true))
        {
            log_web(F("Failed to publish homeassistant %s entity auto-discover"), uniqueName.c_str());
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

        if (!publish_ha_binary_sensor_auto_discover(F("mode_defrost")))
            anyFailed = true;

        if (!publish_ha_float_sensor_auto_discover(F("compressor_frequency"), SensorType::FREQUENCY))
            anyFailed = true;

        if (!publish_ha_binary_sensor_auto_discover(F("mode_dhw_boost")))
            anyFailed = true;

        if (!publish_ha_float_sensor_auto_discover(F("legionella_prevention_temp"), SensorType::TEMPERATURE))
            anyFailed = true;

        if (!publish_ha_float_sensor_auto_discover(F("dhw_temp_drop"), SensorType::TEMPERATURE))
            anyFailed = true;

        if (!publish_ha_float_sensor_auto_discover(F("outside_temp"), SensorType::TEMPERATURE))
            anyFailed = true;

        if (!publish_ha_float_sensor_auto_discover(F("dhw_feed_temp"), SensorType::TEMPERATURE))
            anyFailed = true;

        if (!publish_ha_float_sensor_auto_discover(F("dhw_return_temp"), SensorType::TEMPERATURE))
            anyFailed = true;

        if (!publish_ha_float_sensor_auto_discover(F("boiler_flow_temp"), SensorType::TEMPERATURE))
            anyFailed = true;

        if (!publish_ha_float_sensor_auto_discover(F("boiler_return_temp"), SensorType::TEMPERATURE))
            anyFailed = true;

        if (!publish_ha_float_sensor_auto_discover(F("dhw_flow_temp_target"), SensorType::TEMPERATURE))
            anyFailed = true;

        if (!publish_ha_float_sensor_auto_discover(F("sh_flow_temp_target"), SensorType::TEMPERATURE))
            anyFailed = true;

        if (!publish_ha_string_sensor_auto_discover(F("mode_power")))
            anyFailed = true;

        if (!publish_ha_string_sensor_auto_discover(F("mode_operation")))
            anyFailed = true;

        if (!publish_ha_string_sensor_auto_discover(F("mode_dhw")))
            anyFailed = true;

        if (!publish_ha_string_sensor_auto_discover(F("mode_heating")))
            anyFailed = true;

        if (!publish_ha_float_sensor_auto_discover(F("heating_consumed"), SensorType::POWER))
            anyFailed = true;

        if (!publish_ha_float_sensor_auto_discover(F("heating_delivered"), SensorType::POWER))
            anyFailed = true;

        if (!publish_ha_float_sensor_auto_discover(F("dhw_consumed"), SensorType::POWER))
            anyFailed = true;

        if (!publish_ha_float_sensor_auto_discover(F("dhw_delivered"), SensorType::POWER))
            anyFailed = true;

        if (!anyFailed)
        {
            needsAutoDiscover = false;
            log_web(F("Published homeassistant auto-discovery topics"));
        }
    }

    void publish_climate_status()
    {
        DynamicJsonDocument json(1024);

        {
            auto& status = hp::get_status();
            std::lock_guard<hp::Status> lock{status};

            json[F("temperature")] = round2(status.Zone1SetTemperature);
            json[F("curr_temp")] = round2(status.Zone1RoomTemperature);
            json[F("mode")] = status.ha_mode_as_string();
            json[F("action")] = status.ha_action_as_string();
        }

        const auto& config = config_instance();
        String stateTopic = config.MqttTopic + "/" + unique_entity_name(F("climate_control")) + F("/state");
        if (!publish_mqtt(stateTopic, json))
            log_web(F("Failed to publish MQTT state for: %s"), unique_entity_name(F("climate_control")));
    }

    void publish_binary_sensor_status(const String& name, bool on)
    {
        String state = on ? F("on") : F("off");
        const auto& config = config_instance();
        String stateTopic = config.MqttTopic + "/" + unique_entity_name(name) + F("/state");
        if (!publish_mqtt(stateTopic, state))
            log_web(F("Failed to publish MQTT state for: %s"), unique_entity_name(name));
    }

    template <typename T>
    void publish_sensor_status(const String& name, T value)
    {
        const auto& config = config_instance();
        String stateTopic = config.MqttTopic + "/" + unique_entity_name(name) + F("/state");
        if (!publish_mqtt(stateTopic, String(value)))
            log_web(F("Failed to publish MQTT state for: %s"), unique_entity_name(name));
    }

    void publish_entity_state_updates()
    {
        publish_climate_status();

        auto& status = hp::get_status();
        std::lock_guard<hp::Status> lock{status};
        publish_binary_sensor_status(F("mode_defrost"), status.DefrostActive);
        publish_sensor_status<float>(F("compressor_frequency"), status.CompressorFrequency);
        publish_binary_sensor_status(F("mode_dhw_boost"), status.DhwBoostActive);
        publish_sensor_status<float>(F("legionella_prevention_temp"), status.LegionellaPreventionSetPoint);
        publish_sensor_status<float>(F("dhw_temp_drop"), status.DhwTemperatureDrop);
        publish_sensor_status<float>(F("outside_temp"), status.OutsideTemperature);
        publish_sensor_status<float>(F("dhw_feed_temp"), status.DhwFeedTemperature);
        publish_sensor_status<float>(F("dhw_return_temp"), status.DhwReturnTemperature);
        publish_sensor_status<float>(F("boiler_flow_temp"), status.BoilerFlowTemperature);
        publish_sensor_status<float>(F("boiler_return_temp"), status.BoilerReturnTemperature);
        publish_sensor_status<float>(F("dhw_flow_temp_target"), status.DhwFlowTemperatureSetPoint);
        publish_sensor_status<float>(F("sh_flow_temp_target"), status.RadiatorFlowTemperatureSetPoint);
        publish_sensor_status<String>(F("mode_power"), status.power_as_string());
        publish_sensor_status<String>(F("mode_operation"), status.operation_as_string());
        publish_sensor_status<String>(F("mode_dhw"), status.dhw_mode_as_string());
        publish_sensor_status<String>(F("mode_heating"), status.heating_mode_as_string());
        publish_sensor_status<float>(F("heating_consumed"), status.EnergyConsumedHeating);
        publish_sensor_status<float>(F("heating_delivered"), status.EnergyDeliveredHeating);
        publish_sensor_status<float>(F("dhw_consumed"), status.EnergyConsumedDhw);
        publish_sensor_status<float>(F("dhw_delivered"), status.EnergyDeliveredDhw);
    }

    bool connect()
    {
        if (mqttClient.connected())
            return true;

        Config& config = config_instance();
        if (!config.MqttPassword.isEmpty() && !config.MqttUserName.isEmpty())
        {
            log_web(F("MQTT user '%s' has configured password, connecting with credentials..."), config.MqttUserName.c_str());
            if (!mqttClient.connect(WiFi.localIP().toString().c_str(), config.MqttUserName.c_str(), config.MqttPassword.c_str()))
            {
                log_web(F("MQTT connection failure: '%s'"), get_connection_error_string());
                return false;
            }
        }
        else
        {
            log_web(F("MQTT username/password not configured, connecting as anonymous user..."));
            if (!mqttClient.connect(WiFi.localIP().toString().c_str()))
            {
                log_web(F("MQTT connection failure: '%s'"), get_connection_error_string());
                return false;
            }
        }

        if (mqttClient.connected())
        {
            String tempCmdTopic = config.MqttTopic + "/" + unique_entity_name(F("climate_control")) + F("/temp_cmd");
            if (!mqttClient.subscribe(tempCmdTopic.c_str()))
            {
                log_web(F("Failed to subscribe to temperature command topic!"));
                return false;
            }

            log_web(F("Successfully established MQTT client connection!"));
        }

        return true;
    }

    bool initialize()
    {
        log_web(F("Initializing MQTT..."));

        if (!is_configured())
        {
            log_web(F("Unable to initialize MQTT, server is not configured."));
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

        if (mqttClient.connected())
        {
            publish_homeassistant_auto_discover();
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
                publish_entity_state_updates();
            }
        }

        mqttClient.loop();
    }

    bool is_connected()
    {
        return mqttClient.connected();
    }
} // namespace ehal::mqtt