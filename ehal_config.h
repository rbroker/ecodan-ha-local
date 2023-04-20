#pragma once

namespace ehal
{
    struct Config
    {
        bool FirstTimeConfig;
        String DevicePassword;
        String WifiSsid;
        String WifiPassword;
        String HostName;
        String MqttServer;
        uint16_t MqttPort;
        String MqttUserName;
        String MqttPassword;
        String MqttTopic;
    };
}