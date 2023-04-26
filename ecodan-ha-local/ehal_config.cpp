#include "ehal_config.h"

#include <Preferences.h>

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
        prefs.begin(F("config"), true);

        Config& config = config_instance();
        config.DevicePassword = prefs.getString(F("device_pw"));     
        config.SerialRxPort = prefs.getUShort(F("serial_rx"), 33U);
        config.SerialTxPort = prefs.getUShort(F("serial_tx"), 34U);
        config.DumpPackets = prefs.getBool(F("dump_pkt"), false);
        config.HostName = prefs.getString(F("hostname"), F("ecodan_ha_local"));
        config.WifiSsid = prefs.getString(F("wifi_ssid"));
        config.WifiPassword = prefs.getString(F("wifi_pw"));
        config.HostName = prefs.getString(F("hostname"), F("ecodan_ha_local"));
        config.MqttServer = prefs.getString(F("mqtt_server"));
        config.MqttPort = prefs.getUShort(F("mqtt_port"), 1883U);
        config.MqttUserName = prefs.getString(F("mqtt_username"));
        config.MqttPassword = prefs.getString(F("mqtt_pw"));
        config.MqttTopic = prefs.getString(F("mqtt_topic"), F("ecodan_hp"));

        prefs.end();

        return true;
    }

    bool save_configuration(const Config& config)
    {
        Preferences prefs;
        prefs.begin(F("config"), /* readonly = */ false);
        prefs.putString(F("device_pw"), config.DevicePassword);  
        prefs.putUShort(F("serial_rx"), config.SerialRxPort);
        prefs.putUShort(F("serial_tx"), config.SerialTxPort);  
        prefs.putBool(F("dump_pkt"), config.DumpPackets);    
        prefs.putString(F("wifi_ssid"), config.WifiSsid);
        prefs.putString(F("wifi_pw"), config.WifiPassword);
        prefs.putString(F("hostname"), config.HostName);
        prefs.putString(F("mqtt_server"), config.MqttServer);
        prefs.putUShort(F("mqtt_port"), config.MqttPort);
        prefs.putString(F("mqtt_username"), config.MqttUserName);
        prefs.putString(F("mqtt_pw"), config.MqttPassword);
        prefs.putString(F("mqtt_topic"), config.MqttTopic);
        prefs.end();

        return true;
    }

    bool clear_configuration()
    {
        Preferences prefs;
        prefs.begin(F("config"), /* readonly = */ false);
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
        return F("v0.0.1");
    }

} // namespace ehal