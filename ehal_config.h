#pragma once

namespace ehal
{
    struct Config
    {
        bool FirstTimeConfig;
        String HostName;
        String WifiSsid;
        String WifiPassword;
        String MqttServer;
        uint16_t MqttPort;
        String MqttUserName;
        String MqttPassword;
        String MqttTopic;
    };
}