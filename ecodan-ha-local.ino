#include <WiFi.h>
#include <pgmspace.h>

#include <chrono>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>

#include "ehal_config.h"
#include "ehal_diagnostics.h"
#include "ehal_http.h"
#include "ehal_thirdparty.h"

bool initialize_wifi_access_point()
{
    ehal::log_web("Initializing WiFi connection...");

    ehal::Config& config = ehal::config_instance();

    if (!ehal::requires_first_time_configuration())
    {
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

void setup()
{
    if (!ehal::load_saved_configuration())
    {
        ehal::log_web("Failed to load configuration!");
        return;
    }

    ehal::log_web("Configuration parameters loaded from NVS");

    initialize_wifi_access_point();

    if (ehal::requires_first_time_configuration())
    {
        ehal::log_web("First time configuration required, starting captive portal...");

        ehal::http::initialize_captive_portal();
    }
    else
    {
        ehal::log_web("Regular startup mode, initializing web-server...");

        ehal::http::initialize_default();
    }

    ehal::log_web("Server startup successful, starting request processing.");
}

void loop()
{
    ehal::http::handle_loop();
}
