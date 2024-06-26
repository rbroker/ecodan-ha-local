#include "ehal_config.h"
#include "ehal.h"

#include <Preferences.h>

#ifndef LED_BUILTIN
    #define LED_BUILTIN 15
#endif

namespace ehal
{
    Config& config_instance()
    {
        static Config s_configuration = {};
        return s_configuration;
    }

    bool load_saved_configuration()
    {
        Preferences prefs;
        prefs.begin("config", true);

        Config& config = config_instance();
        config.DevicePassword = prefs.getString("device_pw");
        config.SerialRxPort = prefs.getUShort("serial_rx", 27U);
        config.SerialTxPort = prefs.getUShort("serial_tx", 26U);
        config.StatusLed = prefs.getUShort("status_led", LED_BUILTIN);
        config.DumpPackets = prefs.getBool("dump_pkt", false);
        config.CoolEnabled = prefs.getBool("cool_enabled", false);
        config.UniqueId = prefs.getString("unique_id", device_mac());
        config.WifiReset = prefs.getBool("wifi_reset", true);
        config.HostName = prefs.getString("hostname", "ecodan_ha_local");
        config.WifiSsid = prefs.getString("wifi_ssid");
        config.WifiPassword = prefs.getString("wifi_pw");
        config.HostName = prefs.getString("hostname", "ecodan_ha_local");
        config.MqttServer = prefs.getString("mqtt_server");
        config.MqttPort = prefs.getUShort("mqtt_port", 1883U);
        config.MqttUserName = prefs.getString("mqtt_username");
        config.MqttPassword = prefs.getString("mqtt_pw");
        config.MqttTopic = prefs.getString("mqtt_topic", "ecodan_hp");

        prefs.end();

        return true;
    }

    bool save_configuration(const Config& config)
    {
        Preferences prefs;
        prefs.begin("config", /* readonly = */ false);
        prefs.putString("device_pw", config.DevicePassword);
        prefs.putUShort("serial_rx", config.SerialRxPort);
        prefs.putUShort("serial_tx", config.SerialTxPort);
        prefs.putUShort("status_led", config.StatusLed);
        prefs.putBool("dump_pkt", config.DumpPackets);
        prefs.putBool("cool_enabled", config.CoolEnabled);
        prefs.putString("unique_id", config.UniqueId);
        prefs.putBool("wifi_reset", config.WifiReset);
        prefs.putString("wifi_ssid", config.WifiSsid);
        prefs.putString("wifi_pw", config.WifiPassword);
        prefs.putString("hostname", config.HostName);
        prefs.putString("mqtt_server", config.MqttServer);
        prefs.putUShort("mqtt_port", config.MqttPort);
        prefs.putString("mqtt_username", config.MqttUserName);
        prefs.putString("mqtt_pw", config.MqttPassword);
        prefs.putString("mqtt_topic", config.MqttTopic);
        prefs.end();

        return true;
    }

    bool clear_configuration()
    {
        Preferences prefs;
        prefs.begin("config", /* readonly = */ false);
        prefs.clear();
        prefs.end();

        return true;
    }

    bool requires_first_time_configuration()
    {
        Config& config = config_instance();
        return config.WifiSsid.isEmpty() || config.WifiPassword.isEmpty();
    }

    String get_software_version()
    {
        return FPSTR("v0.2.0");
    }

} // namespace ehal