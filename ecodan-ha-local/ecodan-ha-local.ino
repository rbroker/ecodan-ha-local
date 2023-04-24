#include <WiFi.h>   
#include <time.h>

#include <chrono>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>

#include "ehal.h"
#include "ehal_config.h"
#include "ehal_diagnostics.h"
#include "ehal_hp.h"
#include "ehal_http.h"
#include "ehal_mqtt.h"
#include "ehal_thirdparty.h"

bool mqttInitialized = false;
bool heatpumpInitialized = false;

bool initialize_wifi_access_point()
{
    ehal::log_web("Initializing WiFi connection...");

    ehal::Config& config = ehal::config_instance();

    if (!ehal::requires_first_time_configuration())
    {
        if (!config.HostName.isEmpty())
        {
            if (!WiFi.setHostname(config.HostName.c_str()))
            {
                ehal::log_web("Failed to configure hostname from saved settings!");
            }
        }
        WiFi.begin(config.WifiSsid.c_str(), config.WifiPassword.c_str());
    }

    if (ehal::requires_first_time_configuration())
    {
        if (!WiFi.softAP(config.HostName.c_str(), config.WifiPassword.c_str()))
        {
            ehal::log_web("Unable to create WiFi Access point!");
            return false;
        }
    }

    while (WiFi.isConnected())
    {
        ehal::log_web("Waiting 500ms for WiFi connection...");
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    WiFi.setAutoReconnect(true);

    ehal::log_web("WiFi connection established!");
    return true;
}

void update_time(bool force)
{
    static auto last_time_update = std::chrono::steady_clock::now();

    auto now = std::chrono::steady_clock::now();
    if (force || now - last_time_update > (std::chrono::hours(24) + std::chrono::seconds(5)))
    {
        last_time_update = now;
        configTzTime("UTC0", "pool.ntp.org");
        ehal::log_web("Updated UTC time from NTP");
    }
}

void setup()
{
    if (!ehal::load_saved_configuration())
    {
        ehal::log_web("Failed to load configuration!");
        return;
    }

    ehal::log_web("Configuration parameters loaded from NVS");

    initialize_wifi_access_point();

    update_time(/* force =*/true);

    if (ehal::requires_first_time_configuration())
    {
        ehal::log_web("First time configuration required, starting captive portal...");

        ehal::http::initialize_captive_portal();
    }
    else
    {
        ehal::http::initialize_default();
        heatpumpInitialized = ehal::hp::initialize();
        mqttInitialized = ehal::mqtt::initialize();
    }    

    ehal::log_web("Ecodan HomeAssistant Bridge startup successful, starting request processing.");
}

void loop()
{
    ehal::http::handle_loop();

    if (heatpumpInitialized)
        ehal::hp::handle_loop();

    if (mqttInitialized)
        ehal::mqtt::handle_loop();

    update_time(/* force =*/false);
    delay(1);
}
