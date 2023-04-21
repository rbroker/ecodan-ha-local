#pragma once

#include <Arduino.h>
#include <cstdint>

namespace ehal
{
    struct Config
    {
        String DevicePassword;
        String TimeZone;
        String WifiSsid;
        String WifiPassword;
        String HostName;
        String MqttServer;
        uint16_t MqttPort;
        String MqttUserName;
        String MqttPassword;
        String MqttTopic;
    };

    Config& config_instance();
    bool load_saved_configuration();
    bool save_configuration(const Config& configuration);
    bool clear_configuration();
    bool requires_first_time_configuration();
} // namespace ehal