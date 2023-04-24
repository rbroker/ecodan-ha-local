#include "ehal_config.h"
#include "ehal_css.h"
#include "ehal_diagnostics.h"
#include "ehal_hp.h"
#include "ehal_html.h"
#include "ehal_http.h"
#include "ehal_js.h"
#include "ehal_mqtt.h"
#include "ehal_thirdparty.h"

#include <DNSServer.h>
#include <Update.h>
#include <WebServer.h>

#include <chrono>
#include <thread>

namespace ehal::http
{
    WebServer server(80);
    std::unique_ptr<DNSServer> dnsServer;
    String loginCookie;
    uint8_t failedLoginCount = 0;

    String bool_to_emoji(bool value)
    {
        if (value)
            return F("&#x2705;");
        else
            return F("&#x274C;");
    }

    String uint64_to_string(uint64_t value)
    {
        char buffer[20] = {};
        snprintf(buffer, sizeof(buffer), "%lld", value);
        return buffer;
    }

    String configuration_status()
    {
        Config& config = config_instance();

        if (config.WifiSsid.isEmpty() || config.WifiPassword.isEmpty())
            return F("&#x26A0; WiFi configuration is incomplete!");

        if (config.MqttServer.isEmpty() || config.MqttPort == 0 || config.MqttUserName.isEmpty() || config.MqttPassword.isEmpty() || config.MqttTopic.isEmpty())
            return F("&#x26A0; MQTT configuration is incomplete!");

        if (config.DevicePassword.isEmpty())
            return F("&#x1F513; Setting a Device Password is recommended!");

        if (config.HostName != WiFi.getHostname())
            return F("&#x26A0; Configured Hostname is not being used!");

        return F("&#x2705;");
    }

    String generate_login_cookie()
    {
        String payload = config_instance().DevicePassword + String(xTaskGetTickCount());
        uint8_t sha256[32];

        mbedtls_md_context_t ctx;

        mbedtls_md_init(&ctx);
        mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 0);
        mbedtls_md_starts(&ctx);
        mbedtls_md_update(&ctx, (const unsigned char*)payload.c_str(), payload.length());
        mbedtls_md_finish(&ctx, sha256);
        mbedtls_md_free(&ctx);

        String cookie = "";
        char hex[3] = {};
        for (int i = 0; i < sizeof(sha256); ++i)
        {
            snprintf(hex, sizeof(hex), "%02x", sha256[i]);
            cookie += hex;
        }

        return cookie;
    }

    bool show_login_if_required()
    {
        if (requires_first_time_configuration())
        {
            log_web("Skipping login, as first time configuration is required");
            return false;
        }

        if (config_instance().DevicePassword.isEmpty())
        {
            log_web("Skipping login, as device password is unset.");
            return false;
        }

        if (!loginCookie.isEmpty())
        {
            String clientCookie = server.header("Cookie");
            if (clientCookie.indexOf(String("login-cookie=") + loginCookie) != -1)
            {
                return false;
            }

            log_web("Client cookie mismatch, redirecting to login page");
        }
        else
        {
            log_web("Login cookie is unset, redirecting to login page");
        }

        String page{FPSTR(PAGE_TEMPLATE)};
        page.replace(F("{{PAGE_SCRIPT}}"), "");
        page.replace(F("{{PAGE_BODY}}"), FPSTR(BODY_TEMPLATE_LOGIN));
        server.send(200, F("text/html"), page);
        return true;
    }

    void async_restart()
    {
        std::thread asyncRestart([]()
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            ESP.restart(); 
        });

        asyncRestart.detach();
    }

    void handle_root()
    {
        if (show_login_if_required())
            return;

        String page{FPSTR(PAGE_TEMPLATE)};
        page.replace(F("{{PAGE_SCRIPT}}"), "");
        page.replace(F("{{PAGE_BODY}}"), FPSTR(BODY_TEMPLATE_HOME));
        page.replace(F("{{hp_conn}}"), bool_to_emoji(hp::is_connected()));
        page.replace(F("{{mqtt_conn}}"), bool_to_emoji(mqtt::is_connected()));
        page.replace(F("{{config}}"), configuration_status());
        server.send(200, "text/html", page);
    }

    void handle_configure()
    {
        if (show_login_if_required())
            return;

        String page{FPSTR(PAGE_TEMPLATE)};
        page.replace(F("{{PAGE_SCRIPT}}"), F("src='/configuration.js'"));
        page.replace(F("{{PAGE_BODY}}"), FPSTR(BODY_TEMPLATE_CONFIG));

        Config& config = config_instance();
        page.replace(F("{{device_pw}}"), config.DevicePassword);
        page.replace(F("{{serial_rx}}"), String(config.SerialRxPort));
        page.replace(F("{{serial_tx}}"), String(config.SerialTxPort));
        page.replace(F("{{wifi_ssid}}"), config.WifiSsid);
        page.replace(F("{{wifi_pw}}"), config.WifiPassword);
        page.replace(F("{{hostname}}"), config.HostName);
        page.replace(F("{{mqtt_server}}"), config.MqttServer);
        page.replace(F("{{mqtt_port}}"), String(config.MqttPort));
        page.replace(F("{{mqtt_user}}"), config.MqttUserName);
        page.replace(F("{{mqtt_pw}}"), config.MqttPassword);
        page.replace(F("{{mqtt_topic}}"), config.MqttTopic);

        server.send(200, F("text/html"), page);
    }

    void handle_configuration_js()
    {
        String js{FPSTR(SCRIPT_CONFIGURATION_PAGE)};

        Config& config = config_instance();
        js.replace(F("{{wifi_ssid}}"), config.WifiSsid);

        server.send(200, F("text/javascript"), js);
    }

    void handle_save_configuration()
    {
        Config config;
        config.DevicePassword = server.arg("device_pw");
        config.SerialRxPort = server.arg("serial_rx").toInt();
        config.SerialTxPort = server.arg("serial_tx").toInt();
        config.WifiSsid = server.arg("wifi_ssid");
        config.WifiPassword = server.arg("wifi_pw");
        config.HostName = server.arg("hostname");
        config.MqttServer = server.arg("mqtt_server");
        config.MqttPort = server.arg("mqtt_port").toInt();
        config.MqttUserName = server.arg("mqtt_user");
        config.MqttPassword = server.arg("mqtt_pw");
        config.MqttTopic = server.arg("mqtt_topic");
        save_configuration(config);

        String page{FPSTR(PAGE_TEMPLATE)};
        page.replace(F("{{PAGE_SCRIPT}}"), F("defer src='/reboot.js'"));
        page.replace(F("{{PAGE_BODY}}"), FPSTR(BODY_TEMPLATE_CONFIG_SAVED));
        server.sendHeader("Connection", "close");
        server.send(200, F("text/html"), page);

        async_restart();
    }

    void handle_reboot_js()
    {
        String js{FPSTR(SCRIPT_WAIT_REBOOT)};
        server.send(200, F("text/javascript"), js);
    }

    void handle_clear_config()
    {
        clear_configuration();

        String page{FPSTR(PAGE_TEMPLATE)};
        page.replace(F("{{PAGE_SCRIPT}}"), F("defer src='/reboot.js'"));
        page.replace(F("{{PAGE_BODY}}"), FPSTR(BODY_TEMPLATE_CONFIG_CLEARED));
        server.sendHeader("Connection", "close");
        server.send(200, F("text/html"), page);

        async_restart();
    }

    void handle_query_ssid_list()
    {
        DynamicJsonDocument json(1024);
        JsonArray ssids = json.createNestedArray("ssids");
        String jsonOut;

        log_web("Starting WiFi scan...");
        int count = WiFi.scanNetworks();
        log_web("Wifi Scan Result: %s", String(count).c_str());

        for (int i = 0; i < count; ++i)
        {
            log_web("SSID: %s", WiFi.SSID(i).c_str());
            ssids.add(WiFi.SSID(i));
        }

        serializeJson(json, jsonOut);

        WiFi.scanDelete();

        server.send(200, F("text/plain"), jsonOut);
    }

    void handle_query_life()
    {
        server.send(200, F("text/plain"), F("alive"));
    }

    void handle_query_diagnostic_logs()
    {
        server.send(200, F("text/plain"), logs_as_json());
    }

    void handle_diagnostics()
    {
        if (show_login_if_required())
            return;

        String page{FPSTR(PAGE_TEMPLATE)};
        page.replace(F("{{PAGE_SCRIPT}}"), F("src='/diagnostic.js'"));
        page.replace(F("{{PAGE_BODY}}"), FPSTR(BODY_TEMPLATE_DIAGNOSTICS));

        char deviceMac[19] = {};
        snprintf(deviceMac, sizeof(deviceMac), "%#llx", ESP.getEfuseMac());

        page.replace(F("{{sw_ver}}"), get_software_version());
        page.replace(F("{{device_mac}}"), deviceMac);
        page.replace(F("{{device_cpus}}"), String(ESP.getChipCores()));
        page.replace(F("{{device_cpu_freq}}"), String(ESP.getCpuFreqMHz()));
        page.replace(F("{{device_free_heap}}"), String(ESP.getFreeHeap()));
        page.replace(F("{{device_total_heap}}"), String(ESP.getHeapSize()));
        page.replace(F("{{device_min_heap}}"), String(ESP.getMinFreeHeap()));
        page.replace(F("{{device_free_psram}}"), String(ESP.getFreePsram()));
        page.replace(F("{{device_total_psram}}"), String(ESP.getPsramSize()));

        page.replace(F("{{wifi_hostname}}"), WiFi.getHostname());
        page.replace(F("{{wifi_ip}}"), WiFi.localIP().toString());
        page.replace(F("{{wifi_gateway_ip}}"), WiFi.gatewayIP().toString());
        page.replace(F("{{wifi_mac}}"), WiFi.macAddress());
        page.replace(F("{{wifi_mac}}"), WiFi.macAddress());
        page.replace(F("{{wifi_tx_power}}"), String(WiFi.getTxPower()));

        page.replace(F("{{ha_hp_entity}}"), FPSTR("climate.") + ehal::mqtt::unique_entity_name("climate_control"));

        page.replace(F("{{hp_tx_count}}"), uint64_to_string(hp::get_tx_msg_count()));
        page.replace(F("{{hp_rx_count}}"), uint64_to_string(hp::get_rx_msg_count()));

        server.send(200, F("text/html"), page);
    }

    void handle_diagnostic_js()
    {
        String js{FPSTR(SCRIPT_UPDATE_DIAGNOSTIC_LOGS)};
        server.send(200, F("text/javascript"), js);
    }

    void handle_heat_pump()
    {
        if (show_login_if_required())
            return;

        String page{FPSTR(PAGE_TEMPLATE)};
        page.replace(F("{{PAGE_SCRIPT}}"), "");
        page.replace(F("{{PAGE_BODY}}"), FPSTR(BODY_TEMPLATE_HEAT_PUMP));

        {
            auto& status = hp::get_status();
            std::lock_guard<hp::Status> lock{status};

            page.replace(F("{{z1_room_temp}}"), String(status.Zone1RoomTemperature, 1));
            page.replace(F("{{z1_set_temp}}"), String(status.Zone1SetTemperature, 1));
            page.replace(F("{{z2_room_temp}}"), String(status.Zone2RoomTemperature, 1));
            page.replace(F("{{z2_set_temp}}"), String(status.Zone2SetTemperature, 1));

            page.replace(F("{{sh_consumed}}"), String(status.EnergyConsumedHeating));
            page.replace(F("{{sh_delivered}}"), String(status.EnergyDeliveredHeating));
            page.replace(F("{{sh_cop}}"), String(status.EnergyDeliveredHeating / status.EnergyConsumedHeating));
            page.replace(F("{{dhw_consumed}}"), String(status.EnergyConsumedDhw));
            page.replace(F("{{dhw_delivered}}"), String(status.EnergyDeliveredDhw));
            page.replace(F("{{dhw_cop}}"), String(status.EnergyDeliveredDhw / status.EnergyConsumedDhw));

            page.replace(F("{{mode_pwr}}"), status.power_as_string());
            page.replace(F("{{mode_op}}"), status.operation_as_string());
            page.replace(F("{{mode_hol}}"), bool_to_emoji(status.HolidayMode));
            page.replace(F("{{mode_dhw_timer}}"), bool_to_emoji(status.DhwTimerMode));
            page.replace(F("{{mode_heating}}"), status.heating_mode_as_string());
            page.replace(F("{{mode_dhw}}"), status.dhw_mode_as_string());

            page.replace(F("{{min_flow_temp}}"), String(status.MinimumFlowTemperature));
            page.replace(F("{{max_flow_temp}}"), String(status.MaximumFlowTemperature));
        }

        server.send(200, F("text/html"), page);
    }

    void handle_firmware_update()
    {
        String page{FPSTR(PAGE_TEMPLATE)};
        page.replace(F("{{PAGE_SCRIPT}}"), F("defer src='/reboot.js'"));
        page.replace(F("{{PAGE_BODY}}"), FPSTR(BODY_TEMPLATE_FIRMWARE_UPDATE));

        server.sendHeader("Connection", "close");
        server.send(200, F("text/html"), page);

        async_restart();
    }

    void handle_firmware_update_handler()
    {
        HTTPUpload& upload = server.upload();
        switch (upload.status)
        {
        case UPLOAD_FILE_START:
        {
            if (!Update.begin(UPDATE_SIZE_UNKNOWN))
            {
                log_web("Failed to start firmware update: %s", Update.errorString());
            }
        }
        break;

        case UPLOAD_FILE_WRITE:
        {
            if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
            {
                log_web("Failed to write firmware chunk: %s", Update.errorString());
            }
        }
        break;

        case UPLOAD_FILE_END:
        {
            if (!Update.end(true))
            {
                log_web("Failed to finalize firmware write: %s", Update.errorString());
            }
        }
        break;

        case UPLOAD_FILE_ABORTED:
        {
            log_web("Firmware update process aborted!");
        }
        break;
        }
    }

    void handle_redirect(const char* uri)
    {
        String page{FPSTR(PAGE_TEMPLATE)};
        page.replace(F("{{PAGE_SCRIPT}}"), F("src='/redirect.js'"));
        page.replace(F("{{PAGE_BODY}}"), FPSTR(BODY_TEMPLATE_REDIRECT));
        page.replace(F("{{uri}}"), uri);
        server.send(200, F("text/html"), page);
    }

    void handle_redirect_js()
    {
        String js{FPSTR(SCRIPT_REDIRECT)};
        server.send(200, F("text/javascript"), js);
    }

    void handle_verify_login()
    {
        if (server.arg("device_pw") == config_instance().DevicePassword)
        {
            if (loginCookie.isEmpty())
            {
                loginCookie = generate_login_cookie();
            }

            log_web("Successful device login, authorising client.");

            server.sendHeader("Set-Cookie", String("login-cookie=") + loginCookie);

            // Reset brute-force mitigation count, to avoid upsetting legitimate users.
            failedLoginCount = 0;
        }
        else
        {
            log_web("Device password mismatch! Login attempted with '%s'", server.arg("device_pw").c_str());

            // Mitigate password brute-force.
            if (++failedLoginCount > 5)
            {
                std::this_thread::sleep_for(std::chrono::seconds(5));
                failedLoginCount = 10; // Prevent overflow of counter.
            }
        }

        handle_redirect("/");
    }

    void handle_milligram_css()
    {
        server.send(200, "text/css", FPSTR(ehal::PAGE_CSS));
    }

    void do_common_initialization()
    {
        // Common pages
        server.on("/diagnostics", handle_diagnostics);
        server.on("/configuration", handle_configure);
        server.on("/heat_pump", handle_heat_pump);

        // Forms / magic URIs for buttons.
        server.on("/verify_login", handle_verify_login);
        server.on("/save", handle_save_configuration);
        server.on("/clear_config", handle_clear_config);
        server.on("/update", HTTP_POST, handle_firmware_update, handle_firmware_update_handler);

        // XMLHTTPRequest / Javascript / CSS
        server.on("/query_ssid", handle_query_ssid_list);
        server.on("/query_life", handle_query_life);
        server.on("/query_diagnostic_logs", handle_query_diagnostic_logs);
        server.on("/configuration.js", handle_configuration_js);
        server.on("/reboot.js", handle_reboot_js);
        server.on("/diagnostic.js", handle_diagnostic_js);
        server.on("/redirect.js", handle_redirect_js);
        server.on("/milligram.css", handle_milligram_css);

        const char* headers[] = {"Cookie"};
        server.collectHeaders(headers, sizeof(headers) / sizeof(char*));
        server.begin();
    }

    bool initialize_default()
    {
        ehal::log_web("Regular startup mode, initializing web-server...");

        server.on("/", handle_root);
        server.onNotFound(handle_root);
        do_common_initialization();
        return true;
    }

    bool initialize_captive_portal()
    {
        dnsServer.reset(new DNSServer());
        if (!dnsServer)
        {
            log_web("Failed to allocate DNS server!");
            return false;
        }

        if (!dnsServer->start(/*port =*/53, "*", WiFi.softAPIP()))
        {
            log_web("Failed to start DNS server!");
            return false;
        }

        log_web("Initialized DNS server for captive portal.");

        server.on("/", handle_configure);
        server.onNotFound(handle_configure);
        do_common_initialization();
        return true;
    }

    void handle_loop()
    {
        if (dnsServer && requires_first_time_configuration())
        {
            dnsServer->processNextRequest();
        }

        server.handleClient();
    }
} // namespace ehal::http