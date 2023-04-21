#include "ehal_config.h"
#include "ehal_diagnostics.h"
#include "ehal_mqtt.h"
#include "ehal_thirdparty.h"

#include <WiFiClient.h>

#include <chrono>
#include <thread>

namespace ehal::mqtt
{
    WiFiClient espClient;
    PubSubClient mqttClient(espClient);

    void mqtt_callback(const char* topic, byte* payload, uint length)
    {
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

    bool is_recoverable_connection_failure()
    {
        switch (mqttClient.state())
        {
        case MQTT_CONNECTION_LOST:
        case MQTT_DISCONNECTED:
            log_web("Recoverable MQTT connection failure detected: %s", get_connection_error_string());
            return true;
        default:
            return false;
        }
    }

    bool connect()
    {
        if (mqttClient.connected())
            return true;

        Config& config = config_instance();
        if (!config.MqttPassword.isEmpty() && !config.MqttUserName.isEmpty())
        {
            log_web("MQTT user '%s' has configured password, connecting with credentials...", config.MqttUserName.c_str());
            if (!mqttClient.connect(config.HostName.c_str(), config.MqttUserName.c_str(), config.MqttPassword.c_str()))
            {
                log_web("MQTT connection failure: '%s'", get_connection_error_string());
                return false;
            }
        }
        else
        {
            log_web("MQTT username/password not configured, connecting as anonymous user...");
            if (!mqttClient.connect(config.HostName.c_str()))
            {
                log_web("MQTT connection failure: '%s'", get_connection_error_string());
                return false;
            }
        }

        log_web("Successfully established MQTT client connection!");
        return true;
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
        connect();
        return true;
    }

    void handle_loop()
    {
        if (is_configured() && is_recoverable_connection_failure())
        {
            connect();

            // If the failure isn't as recoverable as we thought, avoid causing too much interference on the network.
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }
} // namespace ehal::mqtt