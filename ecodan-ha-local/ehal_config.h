#pragma once

#include <Arduino.h>
#include <cstdint>

namespace ehal
{
    struct Config
    {
        String DevicePassword;
        uint16_t SerialRxPort;
        uint16_t SerialTxPort;
        uint16_t StatusLed;
        bool DumpPackets;
        bool CoolEnabled;
        String UniqueId;
        bool WifiReset;
        String WifiSsid;
        String WifiPassword;
        String HostName;
        String MqttServer;
        uint16_t MqttPort;
        String MqttUserName;
        String MqttPassword;
        String MqttTopic;
        String BootTime;
    };

    Config& config_instance();
    bool load_saved_configuration();
    bool save_configuration(const Config& configuration);
    bool clear_configuration();
    bool requires_first_time_configuration();
    String get_software_version();
} // namespace ehal