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

    bool publish_climate_status();
    bool publish_binary_sensor_status(const String& name, bool on);
    template <typename T>
    bool publish_sensor_status(const String& name, T value);
    bool connect_mqtt();

    std::mutex statusUpdateMtx;
    bool needsAutoDiscover = true;
    WiFiClient espClient;
    MQTTClient mqttClient(4096);

    enum class SensorType
    {
        POWER,
        LIVE_POWER,
        FREQUENCY,
        TEMPERATURE,
        FLOW_RATE,
        COP,
        CONNECTIVITY,
        WIFI_SIGNAL,
        WIFI_SSID,
        IP_ADDRESS,
        MAC_ADDRESS
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

        float setTemperature = payload.toFloat();

        if (!hp::set_z1_target_temperature(setTemperature))
        {
            log_web(F("Failed to set z1 target temperature!"));
        }
        else
        {
            {
                auto& status = hp::get_status();
                std::lock_guard<hp::Status> lock{status};
                status.Zone1SetTemperature = setTemperature;
            }

            // Ensure lock is released, because publish_climate_status will attempt
            // to acquire it internally
            publish_climate_status();
        }
    }

    void on_z1_flow_target_temperature_set_command(const String& payload)
    {
        if (payload.isEmpty())
        {
            return;
        }

        float setTemperature = payload.toFloat();

        if (!hp::set_z1_flow_target_temperature(setTemperature))
        {
            log_web(F("Failed to set Z1 flow target temperature!"));
        }
        else
        {
            auto& status = hp::get_status();
            std::lock_guard<hp::Status> lock{status};
            status.Zone1FlowTemperatureSetPoint = setTemperature;

            publish_sensor_status<float>(F("z1_flow_temp_target"), setTemperature);
        }
    }

    void on_dhw_temperature_set_command(const String& payload)
    {
        if (payload.isEmpty())
        {
            return;
        }

        float setTemperature = payload.toFloat();

        if (!hp::set_dhw_target_temperature(setTemperature))
        {
            log_web(F("Failed to set DHW target temperature!"));
        }
        else
        {
            auto& status = hp::get_status();
            std::lock_guard<hp::Status> lock{status};
            status.Zone1SetTemperature = setTemperature;

            publish_sensor_status<float>(F("dhw_flow_temp_target"), setTemperature);
        }
    }

    void on_mode_set_command(const String& payload)
    {
        uint8_t mode = -1;

        if (payload == "Heat Target Temperature")
        {
            mode = static_cast<uint8_t>(hp::Status::HpMode::HEAT_ROOM_TEMP);
        }
        else if (payload == "Heat Flow Temperature")
        {
            mode = static_cast<uint8_t>(hp::Status::HpMode::HEAT_FLOW_TEMP);
        }
        else if (payload == "Heat Compensation Curve")
        {
            mode = static_cast<uint8_t>(hp::Status::HpMode::HEAT_COMPENSATION_CURVE);
        }
        else if (payload == "Cool Target Temperature")
        {
            mode = static_cast<uint8_t>(hp::Status::HpMode::COOL_ROOM_TEMP);
        }
        else if (payload == "Cool Flow Temperature")
        {
            mode = static_cast<uint8_t>(hp::Status::HpMode::COOL_FLOW_TEMP);
        }
        else
        {
            log_web(F("Unexpected mode requested: %s"), payload.c_str());
            return;
        }

        if (!hp::set_hp_mode(mode))
        {
            log_web(F("Failed to set hp heating coling operation mode!"));
        }
        else
        {
            auto& status = hp::get_status();
            std::lock_guard<hp::Status> lock{status};
            status.set_heating_cooling_mode(mode);

            publish_sensor_status<String>(F("mode_heating_cooling"), status.hp_mode_as_string());
        }
    }

    void on_dhw_mode_set_command(const String& payload)
    {
        if (!hp::set_dhw_mode(payload))
        {
            log_web(F("Failed to set DHW mode!"));
        }
        else
        {
            auto& status = hp::get_status();
            std::lock_guard<hp::Status> lock{status};
            if (payload == "eco")
                status.HotWaterMode = hp::Status::DhwMode::ECO;
            else if (payload == "performance")
                status.HotWaterMode = hp::Status::DhwMode::NORMAL;
            else if (payload == "off")
                status.Operation = hp::Status::OperationMode::OFF;

            publish_sensor_status<String>(F("mode_dhw"), status.dhw_mode_as_string());
        }
    }

    void on_force_dhw_command(const String& payload)
    {
        bool forced = payload == "ON";

        if (!hp::set_dhw_force(forced))
        {
            log_web(F("Failed to force DHW: %s"), payload.c_str());
        }
        else
        {
            auto& status = hp::get_status();
            std::lock_guard<hp::Status> lock{status};
            status.DhwForcedActive = forced;
            publish_binary_sensor_status(F("mode_dhw_forced"), forced);
        }
    }

    void on_turn_on_off_command(const String& payload)
    {
      bool turnON = payload == "ON";

      if (!hp::set_power_mode(turnON))
        {
            log_web(F("Failed to set power mode!"));
        }
        else
        {
            auto& status = hp::get_status();
            std::lock_guard<hp::Status> lock{status};
            if (turnON)
            {
                status.Power = hp::Status::PowerMode::ON;
            }
            else
            {
                status.Power = hp::Status::PowerMode::STANDBY;
            }
            publish_sensor_status<String>(F("mode_power"), status.power_as_string());
        }
    }

    void holiday_mode_on_off_command(const String& payload)
    {
      bool turnON = payload == "ON";

      if (!hp::set_holiday_mode(turnON))
        {
            log_web(F("Failed to set holiday mode!"));
        }
        else
        {
            auto& status = hp::get_status();
            std::lock_guard<hp::Status> lock{status};
            if (turnON)
            {
                status.setHolidayMode = hp::Status::HolMode::ON;
            }
            else
            {
                status.setHolidayMode = hp::Status::HolMode::OFF;
            }
            publish_binary_sensor_status(F("holiday_mode"), status.holmode_as_string());
        }
    }

    void mqtt_callback(String& topic, String& payload)
    {
        try
        {
            auto& config = config_instance();
            String climateEntity = unique_entity_name(F("climate_control"));
            String tempCmdTopic = config.MqttTopic + "/" + climateEntity + F("/temp_cmd");
            String z1FlowTargetCmdTopic = config.MqttTopic + "/" + unique_entity_name(F("z1_flow_temp_target")) + F("/set");
            String dhwForceCmdTopic = config.MqttTopic + "/" + unique_entity_name(F("force_dhw")) + F("/set");
            String turnOnOffCmdTopic = config.MqttTopic + "/" + unique_entity_name(F("turn_on_off_hp")) + F("/set");
            String dhwTempCmdTopic = config.MqttTopic + "/" + unique_entity_name(F("dhw_water_heater")) + F("/set");
            String dhwModeCmdTopic = config.MqttTopic + "/" + unique_entity_name(F("dhw_mode")) + F("/set");
            String shModeCmdTopic = config.MqttTopic + "/" + unique_entity_name(F("sh_mode")) + F("/set");
            String setHolidayModeTopic = config.MqttTopic + "/" + unique_entity_name(F("holiday_mode")) + F("/set");
            
            log_web(F("MQTT topic received: %s: '%s'"), topic.c_str(), payload.c_str());

            if (tempCmdTopic == topic)
            {
                on_z1_temperature_set_command(payload);
            }
            else if (z1FlowTargetCmdTopic == topic)
            {
                on_z1_flow_target_temperature_set_command(payload);
            }
            else if (dhwTempCmdTopic == topic)
            {
                on_dhw_temperature_set_command(payload);
            }
            else if (shModeCmdTopic == topic)
            {
                on_mode_set_command(payload);
            }
            else if (dhwModeCmdTopic == topic)
            {
                on_dhw_mode_set_command(payload);
            }
            else if (dhwForceCmdTopic == topic)
            {
                on_force_dhw_command(payload);
            }
            else if (turnOnOffCmdTopic == topic)
            {
                on_turn_on_off_command(payload);
            }
            else if (setHolidayModeTopic == topic)
            {
                holiday_mode_on_off_command(payload);
            }
        }
        catch (std::exception const& ex)
        {
            log_web(F("Exception on MQTT callback: %s"), ex.what());
        }
    }

    String get_connection_error_string()
    {
        switch (mqttClient.lastError())
        {
        case LWMQTT_NETWORK_FAILED_CONNECT:
            return F("LWMQTT_NETWORK_FAILED_CONNECT");
        case LWMQTT_NETWORK_TIMEOUT:
            return F("LWMQTT_NETWORK_TIMEOUT");
        case LWMQTT_NETWORK_FAILED_READ:
            return F("LWMQTT_NETWORK_FAILED_READ");
        case LWMQTT_NETWORK_FAILED_WRITE:
            return F("LWMQTT_NETWORK_FAILED_WRITE");
        case LWMQTT_MISSING_OR_WRONG_PACKET:
            return F("LWMQTT_MISSING_OR_WRONG_PACKET");
        case LWMQTT_CONNECTION_DENIED:
            return F("LWMQTT_CONNECTION_DENIED");
        case LWMQTT_FAILED_SUBSCRIPTION:
            return F("LWMQTT_FAILED_SUBSCRIPTION");
        case LWMQTT_PONG_TIMEOUT:
            return F("LWMQTT_PONG_TIMEOUT");
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
        static auto last_discover = now;

        // Periodically re-publish discovery messages, just in case the broker missed them.
        // This appears to happen if the MQTT connection is re-established after home assistant
        // is restarted, and is still initializing.
        if (now - last_discover > std::chrono::minutes(5))
        {
            last_discover = now;
            needsAutoDiscover = true;
        }

        if ((now - last_attempt < std::chrono::seconds(30)))
            return false;

        last_attempt = now;

        return true;
    }

    String unique_entity_name(const String& name)
    {
        String stringName = name;
        stringName.replace(" ", "_");
        return  stringName + "_" + config_instance().UniqueId;
    }

    void add_discovery_device_object(JsonObject obj)
    {
        JsonObject device = obj["device"].to<JsonObject>();
        JsonArray identifiers = device["ids"].to<JsonArray>();
        identifiers.add(device_mac());

        device[F("name")] = F("Mitsubishi A2W Heat Pump");
        device[F("sw")] = get_software_version();
        device[F("mdl")] = hp::get_device_model();
        device[F("mf")] = F("MITSUBISHI ELECTRIC");
        device[F("cu")] = String(F("http://")) + WiFi.localIP().toString() + F("/configuration");
    }

    bool publish_mqtt(const String& topic, const String& payload, bool retain = false)
    {
        const int RETRY_COUNT = 3;
        for (int i = 0; i < RETRY_COUNT; ++i)
        {
            if (mqttClient.publish(topic, payload, retain, static_cast<int>(LWMQTT_QOS2)))
            {
                return true;
            }
            else
            {
                log_web(F("MQTT publishing failure: '%s': %d"), topic.c_str(), mqttClient.lastError());
            }

            if (!mqttClient.connected())
            {
                log_web(F("MQTT network disconnection detected trying to publish: '%s' attempting to re-connect: %d/%d"), topic.c_str(), i+1, RETRY_COUNT);
                connect_mqtt();
            }
        }

        needsAutoDiscover = true;
        return false;
    }

    bool publish_mqtt(const String& topic, const JsonDocument& json, bool retain = false)
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
        String discoveryTopic = String(F("homeassistant/climate/")) + uniqueName + F("/config");
        String stateTopic = config.MqttTopic + "/" + uniqueName + F("/state");
        String tempCmdTopic = config.MqttTopic + "/" + uniqueName + F("/temp_cmd");

        JsonDocument doc;
        JsonObject payloadJson = doc.to<JsonObject>();
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

        JsonArray modes = payloadJson["modes"].to<JsonArray>();
        modes.add(F("heat"));
        modes.add(F("off"));

        if (!publish_mqtt(discoveryTopic, doc, /* retain =*/true))
        {
            log_web(F("Failed to publish homeassistant climate entity auto-discover"));
            return false;
        }

        return true;
    }

    bool publish_ha_force_dhw_auto_discover()
    {
        // https://www.home-assistant.io/integrations/switch.mqtt/
        String uniqueName = unique_entity_name(F("force_dhw"));

        const auto& config = config_instance();
        String discoveryTopic = String(F("homeassistant/switch/")) + uniqueName + F("/config");
        String stateTopic = config.MqttTopic + "/" + unique_entity_name(F("mode_dhw_forced")) + F("/state");
        String cmdTopic = config.MqttTopic + "/" + uniqueName + F("/set");

        JsonDocument doc;
        JsonObject payloadJson = doc.to<JsonObject>();
        payloadJson[F("name")] = uniqueName;
        payloadJson[F("unique_id")] = uniqueName;
        payloadJson[F("icon")] = F("mdi:toggle-switch-variant");

        add_discovery_device_object(payloadJson);

        payloadJson[F("stat_t")] = stateTopic;
        payloadJson[F("stat_t_tpl")] = F("{{ value }}");
        payloadJson[F("stat_on")] = F("on");
        payloadJson[F("stat_off")] = F("off");
        payloadJson[F("cmd_t")] = cmdTopic;
        payloadJson[F("cmd_tpl")] = F("{{ value }}");

        if (!publish_mqtt(discoveryTopic, doc, /* retain =*/true))
        {
            log_web(F("Failed to publish homeassistant force DHW entity auto-discover"));
            return false;
        }

        return true;
    }

    bool publish_ha_switch_holiday_mode_auto_discover()
    {
        // https://www.home-assistant.io/integrations/switch.mqtt/
        String uniqueName = unique_entity_name(F("holiday_mode"));
        const auto& config = config_instance();
        String discoveryTopic = String(F("homeassistant/switch/")) + uniqueName + F("/config");
        String stateTopic = config.MqttTopic + "/" + uniqueName + F("/state");
        String cmdTopic = config.MqttTopic + "/" + uniqueName + F("/set");

        JsonDocument doc;
        JsonObject payloadJson = doc.to<JsonObject>();
        payloadJson[F("name")] = uniqueName;
        payloadJson[F("unique_id")] = uniqueName;
        payloadJson[F("icon")] = F("mdi:toggle-switch-variant");

        add_discovery_device_object(payloadJson);

        payloadJson[F("stat_t")] = stateTopic;
        payloadJson[F("stat_t_tpl")] = F("{{ value }}");
        payloadJson[F("stat_on")] = F("on");
        payloadJson[F("stat_off")] = F("off");
        payloadJson[F("cmd_t")] = cmdTopic;
        payloadJson[F("cmd_tpl")] = F("{{ value }}");

        if (!publish_mqtt(discoveryTopic, doc, /* retain =*/true))
        {
            log_web(F("Failed to publish homeassistant Holiday Mode entity auto-discover"));
            return false;
        }
        return true;
    }

    bool publish_ha_set_z1_flow_target_auto_discover()
    {
        // https://www.home-assistant.io/integrations/number.mqtt/
        String uniqueName = unique_entity_name(F("z1_flow_temp_target"));

        const auto& config = config_instance();
        auto& status = hp::get_status();
        String discoveryTopic = String(F("homeassistant/number/")) + uniqueName + F("/config");
        String stateTopic = config.MqttTopic + "/" + unique_entity_name(F("z1_flow_temp_target")) + F("/state");
        String cmdTopic = config.MqttTopic + "/" + uniqueName + F("/set");

        JsonDocument doc;
        JsonObject payloadJson = doc.to<JsonObject>();
        payloadJson[F("name")] = uniqueName;
        payloadJson[F("unique_id")] = uniqueName;

        add_discovery_device_object(payloadJson);

        payloadJson[F("stat_t")] = stateTopic;
        payloadJson[F("stat_t_tpl")] = F("{{ value }}");
        payloadJson[F("cmd_t")] = cmdTopic;
        payloadJson[F("cmd_tpl")] = F("{{ value }}");
        payloadJson[F("min")] = String(ehal::hp::get_min_flow_target_temperature(status.hp_mode_as_string()));
        payloadJson[F("max")] = String(ehal::hp::get_max_flow_target_temperature(status.hp_mode_as_string()));
        payloadJson[F("step")] = 1;
        payloadJson[F("dev_cla")] = F("temperature");
        payloadJson[F("unit_of_meas")] = F("°C");
        payloadJson[F("icon")] = String("mdi:thermometer-water");

        if (!publish_mqtt(discoveryTopic, doc, /* retain =*/true))
        {
            log_web(F("Failed to publish homeassistant Z1 flow temperature set entity auto-discover"));
            return false;
        }

        return true;
    }

    bool publish_ha_turn_on_off_auto_discover()
    {
        // https://www.home-assistant.io/integrations/switch.mqtt/
        String uniqueName = unique_entity_name(F("turn_on_off_hp"));

        const auto& config = config_instance();
        String discoveryTopic = String(F("homeassistant/switch/")) + uniqueName + F("/config");
        String stateTopic = config.MqttTopic + "/" + unique_entity_name(F("mode_power")) + F("/state");
        String cmdTopic = config.MqttTopic + "/" + uniqueName + F("/set");

        JsonDocument doc;
        JsonObject payloadJson = doc.to<JsonObject>();
        payloadJson[F("name")] = uniqueName;
        payloadJson[F("unique_id")] = uniqueName;
        payloadJson[F("icon")] = F("mdi:power");

        add_discovery_device_object(payloadJson);

        payloadJson[F("stat_t")] = stateTopic;
        payloadJson[F("stat_t_tpl")] = F("{{ value }}");
        payloadJson[F("stat_on")] = F("On");
        payloadJson[F("stat_off")] = F("Standby");
        payloadJson[F("cmd_t")] = cmdTopic;
        payloadJson[F("cmd_tpl")] = F("{{ value }}");

        if (!publish_mqtt(discoveryTopic, doc, /* retain =*/true))
        {
            log_web(F("Failed to publish homeassistant turn On/Off HP entity auto-discover"));
            return false;
        }

        return true;
    }

    bool publish_ha_set_dhw_temp_auto_discover()
    {
        // https://www.home-assistant.io/integrations/water_heater.mqtt/
        String uniqueName = unique_entity_name(F("dhw_water_heater"));

        const auto& config = config_instance();
        String discoveryTopic = String(F("homeassistant/water_heater/")) + uniqueName + F("/config");

        JsonDocument doc;
        JsonObject payloadJson = doc.to<JsonObject>();
        payloadJson[F("name")] = uniqueName;
        payloadJson[F("unique_id")] = uniqueName;

        add_discovery_device_object(payloadJson);

        payloadJson[F("curr_temp_t")] = config.MqttTopic + "/" + unique_entity_name(F("dhw_temp")) + F("/state");
        payloadJson[F("temp_cmd_t")] = config.MqttTopic + "/" + uniqueName + F("/set");
        payloadJson[F("temp_stat_t")] = config.MqttTopic + "/" + unique_entity_name(F("dhw_flow_temp_target")) + F("/state");
        payloadJson[F("mode_stat_t")] = config.MqttTopic + "/" + unique_entity_name(F("mode_dhw")) + F("/state");
        payloadJson[F("mode_stat_tpl")] = "{% if value==\"Eco\" %} eco {% elif value==\"Normal\" %} performance {% else %} off {% endif %}";
        payloadJson[F("power_cmd_t")] = config.MqttTopic + "/" + unique_entity_name(F("mode_dhw_forced")) + F("/state");
        payloadJson[F("mode_cmd_t")] = config.MqttTopic + "/" + unique_entity_name(F("dhw_mode")) + F("/set");
        payloadJson[F("min_temp")] = String(ehal::hp::get_min_dhw_temperature());
        payloadJson[F("max_temp")] = String(ehal::hp::get_max_dhw_temperature());
        JsonArray modes = payloadJson["modes"].to<JsonArray>();
        modes.add("off");
        modes.add("eco");
        modes.add("performance");
        payloadJson[F("temp_unit")] = "C";
        payloadJson[F("precision")] = 0.5f;

        if (!publish_mqtt(discoveryTopic, doc, /* retain =*/true))
        {
            log_web(F("Failed to publish homeassistant DHW temperature set entity auto-discover"));
            return false;
        }

        return true;
    }

    bool publish_ha_set_sh_mode_auto_discover()
    {
        // https://www.home-assistant.io/integrations/select.mqtt/
        String uniqueName = unique_entity_name(F("sh_mode"));

        const auto& config = config_instance();
        String discoveryTopic = String(F("homeassistant/select/")) + uniqueName + F("/config");

        JsonDocument doc;
        JsonObject payloadJson = doc.to<JsonObject>();
        payloadJson[F("name")] = uniqueName;
        payloadJson[F("unique_id")] = uniqueName;

        add_discovery_device_object(payloadJson);

        payloadJson[F("stat_t")] = config.MqttTopic + "/" + unique_entity_name(F("mode_heating_cooling")) + F("/state");
        payloadJson[F("cmd_t")] = config.MqttTopic + "/" + unique_entity_name(F("sh_mode")) + F("/set");
        JsonArray options = payloadJson["options"].to<JsonArray>();
        options.add("Heat Target Temperature");
        options.add("Heat Flow Temperature");
        options.add("Heat Compensation Curve");
        if (config.CoolEnabled) {
          options.add("Cool Target Temperature");
          options.add("Cool Flow Temperature");
        }

        if (!publish_mqtt(discoveryTopic, doc, /* retain =*/true))
        {
            log_web(F("Failed to publish homeassistant SH mode entity auto-discover"));
            return false;
        }

        return true;
    }

    bool publish_ha_binary_sensor_auto_discover(const String& name)
    {
        const auto& config = config_instance();
        String uniqueName = unique_entity_name(name);
        String discoveryTopic = String(F("homeassistant/binary_sensor/")) + uniqueName + F("/config");
        String stateTopic = config.MqttTopic + "/" + uniqueName + F("/state");

        // https://www.home-assistant.io/integrations/binary_sensor.mqtt/
        JsonDocument doc;
        JsonObject payloadJson = doc.to<JsonObject>();
        payloadJson[F("name")] = uniqueName;
        payloadJson[F("unique_id")] = uniqueName;

        add_discovery_device_object(payloadJson);

        payloadJson[F("stat_t")] = stateTopic;
        payloadJson[F("payload_off")] = F("off");
        payloadJson[F("payload_on")] = F("on");
        payloadJson[F("exp_aft")] = SENSOR_STATE_TIMEOUT;

        if (!publish_mqtt(discoveryTopic, doc))
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
        String discoveryTopic = String(F("homeassistant/sensor/")) + uniqueName + F("/config");
        String stateTopic = config.MqttTopic + "/" + uniqueName + F("/state");

        // https://www.home-assistant.io/integrations/sensor.mqtt/
        JsonDocument doc;
        JsonObject payloadJson = doc.to<JsonObject>();
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

        case SensorType::LIVE_POWER:
            payloadJson[F("unit_of_meas")] = F("kW");
            payloadJson[F("icon")] = F("mdi:lightning-bolt");
            payloadJson[F("dev_cla")] = F("energy");
            break;

        case SensorType::FREQUENCY:
            payloadJson[F("unit_of_meas")] = F("Hz");
            payloadJson[F("icon")] = F("mdi:fan");
            payloadJson[F("dev_cla")] = F("frequency");
            break;

        case SensorType::FLOW_RATE:
            payloadJson[F("unit_of_meas")] = F("L/min");
            payloadJson[F("icon")] = F("mdi:pump");
            // payloadJson[F("dev_cla")] = F("water");
            break;

        case SensorType::TEMPERATURE:
            payloadJson[F("unit_of_meas")] = F("°C");
            payloadJson[F("dev_cla")] = F("temperature");
            break;

        case SensorType::COP:
            payloadJson[F("icon")] = F("mdi:heat-pump-outline");
            payloadJson[F("unit_of_meas")] = F("COP");
            payloadJson[F("stat_cla")] = F("measurement");
            break;

        default:
            break;
        }

        if (!publish_mqtt(discoveryTopic, doc))
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
        String discoveryTopic = String(F("homeassistant/sensor/")) + uniqueName + F("/config");
        String stateTopic = config.MqttTopic + "/" + uniqueName + F("/state");

        // https://www.home-assistant.io/integrations/sensor.mqtt/
        JsonDocument doc;
        JsonObject payloadJson = doc.to<JsonObject>();
        payloadJson[F("name")] = uniqueName;
        payloadJson[F("unique_id")] = uniqueName;

        add_discovery_device_object(payloadJson);

        payloadJson[F("stat_t")] = stateTopic;
        payloadJson[F("val_tpl")] = F("{{ value }}");
        payloadJson[F("exp_aft")] = SENSOR_STATE_TIMEOUT;

        if (!icon.isEmpty())
        {
            payloadJson[F("icon")] = icon;
        }

        if (!publish_mqtt(discoveryTopic, doc))
        {
            log_web(F("Failed to publish homeassistant %s entity auto-discover"), uniqueName.c_str());
            return false;
        }

        return true;
    }

    bool publish_ha_diagnostic_sensor_auto_discover(const String& name, SensorType type)
    {
        const auto& config = config_instance();
        String uniqueName = unique_entity_name(name);
        String discoveryTopic = String(F("homeassistant/sensor/")) + uniqueName + F("/config");
        String stateTopic = config.MqttTopic + "/" + uniqueName + F("/state");

        // https://www.home-assistant.io/integrations/sensor.mqtt/
        JsonDocument doc;
        JsonObject payloadJson = doc.to<JsonObject>();
        payloadJson[F("name")] = name;
        payloadJson[F("unique_id")] = uniqueName;

        add_discovery_device_object(payloadJson);

        payloadJson[F("entity_category")] = "diagnostic";
        payloadJson[F("stat_t")] = stateTopic;
        payloadJson[F("val_tpl")] = F("{{ value }}");
        payloadJson[F("exp_aft")] = SENSOR_STATE_TIMEOUT;

        switch (type)
        {
        case SensorType::CONNECTIVITY:
            discoveryTopic = String(F("homeassistant/binary_sensor/")) + uniqueName + F("/config");
            payloadJson[F("payload_off")] = F("off");
            payloadJson[F("payload_on")] = F("on");
            payloadJson[F("dev_cla")] = F("connectivity");
            break;
        case SensorType::WIFI_SIGNAL:
            payloadJson[F("unit_of_meas")] = F("dBm");
            payloadJson[F("icon")] = F("mdi:wifi");
            payloadJson[F("dev_cla")] = F("signal_strength");
            break;
        case SensorType::WIFI_SSID:
            payloadJson[F("icon")] = F("mdi:eye");
            payloadJson[F("enabled_by_default")] = bool(false);
            break;
        case SensorType::IP_ADDRESS:
            payloadJson[F("icon")] = F("mdi:ip");
            payloadJson[F("enabled_by_default")] = bool(false);
            break;
        case SensorType::MAC_ADDRESS:
            payloadJson[F("icon")] = F("mdi:eye");
            payloadJson[F("enabled_by_default")] = bool(false);
            break;
        default:
            break;
        }

        if (!publish_mqtt(discoveryTopic, doc))
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

        // https://www.home-assistant.io/integrations/mqtt/
        if (!publish_ha_climate_auto_discover())
            return;

        if (!publish_ha_set_z1_flow_target_auto_discover())
            return;

        if (!publish_ha_force_dhw_auto_discover())
            return;

        if (!publish_ha_turn_on_off_auto_discover())
            return;

        if (!publish_ha_set_dhw_temp_auto_discover())
            return;

        if (!publish_ha_set_sh_mode_auto_discover())
            return;

        if (!publish_ha_switch_holiday_mode_auto_discover())
            return;

        if (!publish_ha_binary_sensor_auto_discover(F("mode_defrost")))
            return;

        if (!publish_ha_float_sensor_auto_discover(F("compressor_frequency"), SensorType::FREQUENCY))
            return;

        if (!publish_ha_float_sensor_auto_discover(F("flow_rate"), SensorType::FLOW_RATE))
            return;

        if (!publish_ha_binary_sensor_auto_discover(F("mode_dhw_forced")))
            return;

        if (!publish_ha_float_sensor_auto_discover(F("output_pwr"), SensorType::LIVE_POWER))
            return;

        if (!publish_ha_float_sensor_auto_discover(F("legionella_prevention_temp"), SensorType::TEMPERATURE))
            return;

        if (!publish_ha_float_sensor_auto_discover(F("dhw_temp_drop"), SensorType::TEMPERATURE))
            return;

        if (!publish_ha_float_sensor_auto_discover(F("outside_temp"), SensorType::TEMPERATURE))
            return;

        if (!publish_ha_float_sensor_auto_discover(F("hp_feed_temp"), SensorType::TEMPERATURE))
            return;

        if (!publish_ha_float_sensor_auto_discover(F("hp_return_temp"), SensorType::TEMPERATURE))
            return;

        if (!publish_ha_float_sensor_auto_discover(F("boiler_flow_temp"), SensorType::TEMPERATURE))
            return;

        if (!publish_ha_float_sensor_auto_discover(F("boiler_return_temp"), SensorType::TEMPERATURE))
            return;

        if (!publish_ha_float_sensor_auto_discover(F("dhw_flow_temp_target"), SensorType::TEMPERATURE))
            return;

        if (!publish_ha_float_sensor_auto_discover(F("sh_flow_temp_target"), SensorType::TEMPERATURE))
            return;

        if (!publish_ha_string_sensor_auto_discover(F("mode_power")))
            return;

        if (!publish_ha_string_sensor_auto_discover(F("mode_operation")))
            return;

        if (!publish_ha_string_sensor_auto_discover(F("mode_dhw")))
            return;

        if (!publish_ha_string_sensor_auto_discover(F("mode_heating_cooling")))
            return;

        if (!publish_ha_float_sensor_auto_discover(F("heating_consumed"), SensorType::POWER))
            return;

        if (!publish_ha_float_sensor_auto_discover(F("heating_delivered"), SensorType::POWER))
            return;

        if (!publish_ha_float_sensor_auto_discover(F("cooling_consumed"), SensorType::POWER))
            return;

        if (!publish_ha_float_sensor_auto_discover(F("cooling_delivered"), SensorType::POWER))
            return;

        if (!publish_ha_float_sensor_auto_discover(F("dhw_consumed"), SensorType::POWER))
            return;

        if (!publish_ha_float_sensor_auto_discover(F("dhw_delivered"), SensorType::POWER))
            return;

        if (!publish_ha_float_sensor_auto_discover(F("z1_room_temp"), SensorType::TEMPERATURE))
            return;

        if (!publish_ha_float_sensor_auto_discover(F("z1_flow_temp_target"), SensorType::TEMPERATURE))
            return;

        if (!publish_ha_float_sensor_auto_discover(F("z1_room_temp_target"), SensorType::TEMPERATURE))
            return;

        if (!publish_ha_float_sensor_auto_discover(F("z2_room_temp"), SensorType::TEMPERATURE))
            return;

        if (!publish_ha_float_sensor_auto_discover(F("z2_flow_temp_target"), SensorType::TEMPERATURE))
            return;

        if (!publish_ha_float_sensor_auto_discover(F("z2_room_temp_target"), SensorType::TEMPERATURE))
            return;

        if (!publish_ha_float_sensor_auto_discover(F("dhw_temp"), SensorType::TEMPERATURE))
            return;

        if (!publish_ha_float_sensor_auto_discover(F("dhw_cop"), SensorType::COP))
            return;

        if (!publish_ha_float_sensor_auto_discover(F("sh_cop"), SensorType::COP))
            return;

        if (!publish_ha_float_sensor_auto_discover(F("cool_cop"), SensorType::COP))
            return;

        if (!publish_ha_binary_sensor_auto_discover(F("holiday_mode")))
            return;
        // Diagnostic sensors
        if (!publish_ha_diagnostic_sensor_auto_discover(F("Heat pump connection state"), SensorType::CONNECTIVITY))
            return;

        if (!publish_ha_diagnostic_sensor_auto_discover(F("Wifi signal"), SensorType::WIFI_SIGNAL))
            return;

        if (!publish_ha_diagnostic_sensor_auto_discover(F("Wifi SSID"), SensorType::WIFI_SSID))
            return;

        if (!publish_ha_diagnostic_sensor_auto_discover(F("IP address"), SensorType::IP_ADDRESS))
            return;

        if (!publish_ha_diagnostic_sensor_auto_discover(F("MAC address"), SensorType::MAC_ADDRESS))
            return;
    }

    bool publish_climate_status()
    {
        JsonDocument doc;
        JsonObject json = doc.to<JsonObject>();


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
        if (!publish_mqtt(stateTopic, doc))
        {
            log_web(F("Failed to publish MQTT state for: %s"), unique_entity_name(F("climate_control")).c_str());
            return false;
        }

        return true;
    }

    bool publish_binary_sensor_status(const String& name, bool on)
    {
        String state = on ? F("on") : F("off");
        const auto& config = config_instance();
        String stateTopic = config.MqttTopic + "/" + unique_entity_name(name) + F("/state");
        if (!publish_mqtt(stateTopic, state))
        {
            log_web(F("Failed to publish MQTT state for: %s"), unique_entity_name(name).c_str());
            return false;
        }

        return true;
    }

    template <typename T>
    bool publish_sensor_status(const String& name, T value)
    {
        const auto& config = config_instance();
        String stateTopic = config.MqttTopic + "/" + unique_entity_name(name) + F("/state");
        if (!publish_mqtt(stateTopic, String(value)))
        {
            log_web(F("Failed to publish MQTT state for: %s"), unique_entity_name(name).c_str());
            return false;
        }

        return true;
    }

    void publish_entity_state_updates()
    {
        if (!mqttClient.connected())
            return;

        if (!publish_climate_status())
            return;

        auto& status = hp::get_status();
        std::lock_guard<hp::Status> lock{status};
        if (!publish_binary_sensor_status(F("mode_defrost"), status.DefrostActive))
            return;

        if (!publish_sensor_status<float>(F("compressor_frequency"), status.CompressorFrequency))
            return;

        if (!publish_sensor_status<float>(F("flow_rate"), status.FlowRate))
            return;

        if (!publish_binary_sensor_status(F("mode_dhw_forced"), status.DhwForcedActive))
            return;

        if (!publish_sensor_status<float>(F("output_pwr"), status.OutputPower))
            return;

        if (!publish_sensor_status<float>(F("legionella_prevention_temp"), status.LegionellaPreventionSetPoint))
            return;

        if (!publish_sensor_status<float>(F("dhw_temp_drop"), status.DhwTemperatureDrop))
            return;

        if (!publish_sensor_status<float>(F("outside_temp"), status.OutsideTemperature))
            return;

        if (!publish_sensor_status<float>(F("hp_feed_temp"), status.DhwFeedTemperature))
            return;

        if (!publish_sensor_status<float>(F("hp_return_temp"), status.DhwReturnTemperature))
            return;

        if (!publish_sensor_status<float>(F("boiler_flow_temp"), status.BoilerFlowTemperature))
            return;

        if (!publish_sensor_status<float>(F("boiler_return_temp"), status.BoilerReturnTemperature))
            return;

        if (!publish_sensor_status<float>(F("dhw_flow_temp_target"), status.DhwFlowTemperatureSetPoint))
            return;

        if (!publish_sensor_status<float>(F("sh_flow_temp_target"), status.RadiatorFlowTemperatureSetPoint))
            return;

        if (!publish_sensor_status<String>(F("mode_power"), status.power_as_string()))
            return;

        if (!publish_sensor_status<String>(F("mode_operation"), status.operation_as_string()))
            return;

        if (!publish_sensor_status<String>(F("mode_dhw"), status.dhw_mode_as_string()))
            return;

        if (!publish_sensor_status<String>(F("mode_heating_cooling"), status.hp_mode_as_string()))
            return;

        if (!publish_sensor_status<float>(F("heating_consumed"), status.EnergyConsumedHeating))
            return;

        if (!publish_sensor_status<float>(F("heating_delivered"), status.EnergyDeliveredHeating))
            return;

        if (!publish_sensor_status<float>(F("cooling_consumed"), status.EnergyConsumedCooling))
            return;

        if (!publish_sensor_status<float>(F("cooling_delivered"), status.EnergyDeliveredCooling))
            return;

        if (!publish_sensor_status<float>(F("dhw_consumed"), status.EnergyConsumedDhw))
            return;

        if (!publish_sensor_status<float>(F("dhw_delivered"), status.EnergyDeliveredDhw))
            return;

        if (!publish_sensor_status<float>(F("z1_room_temp"), status.Zone1RoomTemperature))
            return;

        if (!publish_sensor_status<float>(F("z1_flow_temp_target"), status.Zone1FlowTemperatureSetPoint))
            return;

        if (!publish_sensor_status<float>(F("z1_room_temp_target"), status.Zone1SetTemperature))
            return;

        if (!publish_sensor_status<float>(F("z2_room_temp"), status.Zone2RoomTemperature))
            return;

        if (!publish_sensor_status<float>(F("z2_flow_temp_target"), status.Zone2FlowTemperatureSetPoint))
            return;

        if (!publish_sensor_status<float>(F("z2_room_temp_target"), status.Zone2SetTemperature))
            return;

        if (!publish_sensor_status<float>(F("dhw_temp"), status.DhwTemperature))
            return;

        if (!publish_sensor_status<float>(F("dhw_cop"), status.EnergyConsumedDhw > 0.0f ? status.EnergyDeliveredDhw / status.EnergyConsumedDhw : 0.0f))
            return;

        if (!publish_sensor_status<float>(F("sh_cop"), status.EnergyConsumedHeating > 0.0f ? status.EnergyDeliveredHeating / status.EnergyConsumedHeating : 0.0f))
            return;
        
        if (!publish_sensor_status<float>(F("cool_cop"), status.EnergyConsumedCooling > 0.0f ? status.EnergyDeliveredCooling / status.EnergyConsumedCooling : 0.0f))
            return;

        if (!publish_binary_sensor_status(F("holiday_mode"), status.HolidayMode))
            return;
        // Diagnostic
        if (!publish_binary_sensor_status(F("Heat pump connection state"), hp::is_connected()))
            return;

        if (!publish_sensor_status<int>(F("Wifi signal"), int(WiFi.RSSI())))
            return;

        if (!publish_sensor_status<String>(F("Wifi SSID"), WiFi.SSID()))
            return;

        if (!publish_sensor_status<String>(F("IP address"), WiFi.localIP().toString()))
            return;

        if (!publish_sensor_status<String>(F("MAC address"), WiFi.macAddress()))
            return;
    }

    bool connect_mqtt()
    {
        if (mqttClient.connected())
            return true;

        Config& config = config_instance();
        if (!config.MqttPassword.isEmpty() && !config.MqttUserName.isEmpty())
        {
            log_web(F("MQTT user '%s' has configured password, connecting with credentials..."), config.MqttUserName.c_str());
            int mqtt_connection_retries = 0;
            while (!mqttClient.connect(WiFi.localIP().toString().c_str(), config.MqttUserName.c_str(), config.MqttPassword.c_str())) 
            {
                log_web(F("Connecting to MQTT server ..."));
                delay(1000);
                if (mqtt_connection_retries > 10) 
                {
                    break;
                }
                mqtt_connection_retries++;
            }
            if (mqtt_connection_retries > 10)
            {
                log_web(F("MQTT connection failure: '%s'"), get_connection_error_string().c_str());
                return false;
            }
        }
        else
        {
            log_web(F("MQTT username/password not configured, connecting as anonymous user..."));
            int mqtt_connection_retries = 0;
            while (!mqttClient.connect(WiFi.localIP().toString().c_str())) 
            {
                log_web(F("Connecting to MQTT server ..."));
                delay(1000);
                if (mqtt_connection_retries > 10) 
                {
                    break;
                }
                mqtt_connection_retries++;
            }
            if (mqtt_connection_retries > 10)
            {
                log_web(F("MQTT connection failure: '%s'"), get_connection_error_string().c_str());
                return false;
            }
        }

        if (mqttClient.connected())
        {
            needsAutoDiscover = true;

            String tempCmdTopic = config.MqttTopic + "/" + unique_entity_name(F("climate_control")) + F("/temp_cmd");
            if (!mqttClient.subscribe(tempCmdTopic))
            {
                log_web(F("Failed to subscribe to temperature command topic!"));
                return false;
            }

            if (!mqttClient.subscribe(config.MqttTopic + "/" + unique_entity_name(F("force_dhw")) + F("/set")))
            {
                log_web(F("Failed to subscribe to boost DHW command topic!"));
                return false;
            }

            if (!mqttClient.subscribe(config.MqttTopic + "/" + unique_entity_name(F("turn_on_off_hp")) + F("/set")))
            {
                log_web(F("Failed to subscribe to turn ON/OFF command topic!"));
                return false;
            }

            if (!mqttClient.subscribe(config.MqttTopic + "/" + unique_entity_name(F("dhw_water_heater")) + F("/set")))
            {
                log_web(F("Failed to subscribe to DHW temperature topic!"));
                return false;
            }

            if (!mqttClient.subscribe(config.MqttTopic + "/" + unique_entity_name(F("z1_flow_temp_target")) + F("/set")))
            {
                log_web(F("Failed to subscribe to Z1 flow target temperature command topic!"));
                return false;
            }

            if (!mqttClient.subscribe(config.MqttTopic + "/" + unique_entity_name(F("dhw_mode")) + F("/set")))
            {
                log_web(F("Failed to subscribe to DHW mode command topic!"));
                return false;
            }

            if (!mqttClient.subscribe(config.MqttTopic + "/" + unique_entity_name(F("sh_mode")) + F("/set")))
            {
                log_web(F("Failed to subscribe to SH mode command topic!"));
                return false;
            }

            if (!mqttClient.subscribe(config.MqttTopic + "/" + unique_entity_name(F("holiday_mode")) + F("/set")))
            {
                log_web(F("Failed to subscribe to Holiday Mode command topic!"));
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


        auto& config = config_instance();
        
        const int KEEPALIVE_TIME_SECONDS = 90;
        const bool USE_CLEAN_SESSION = false; // Persistent MQTT session
        const int COMMAND_TIMEOUT_MILLISECONDS = 10000;
        mqttClient.setOptions(KEEPALIVE_TIME_SECONDS, USE_CLEAN_SESSION, COMMAND_TIMEOUT_MILLISECONDS);
        mqttClient.onMessage(mqtt_callback);
        mqttClient.begin(config.MqttServer.c_str(), config.MqttPort, espClient);

        if (connect_mqtt())
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
                // If homeassistant restarts, we'll need to re-publish auto-discovery
                if (!mqttClient.connected())
                {
                    log_web(F("MQTT disconnect detected during periodic update check!"));
                    needsAutoDiscover = true;
                }

                // Re-establish MQTT connection if we need to.
                connect_mqtt();

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
