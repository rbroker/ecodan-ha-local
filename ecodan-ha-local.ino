#include <WebServer.h>
#include <DNSServer.h>
#include <pgmspace.h>
#include <WiFi.h>
#include <Preferences.h>
#include <Update.h>

#include <chrono>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>

#include <ArduinoJson.h>  // ArduinoJson v6.21.2
#include <mbedtls/md.h>   // Seeed_Arduino_mdedtls v3.0.1

#include "ehal_config.h"
#include "ehal_page.h"

const char* SOFTWARE_VERSION PROGMEM = "0.0.1";
WebServer server(80);
ehal::Config config;

std::mutex diagnosticRingbufferLock;
std::deque<String> diagnosticRingbuffer;
std::unique_ptr<DNSServer> dnsServer;
String loginCookie;

void log(String message) {
  const uint8_t MAX_MESSAGE_LENGTH = 255U;

  // If diagnostic message exceeds maximum length, truncate it.
  if (message.length() > MAX_MESSAGE_LENGTH) {
    message = message.substring(0, MAX_MESSAGE_LENGTH);
  }

  std::lock_guard<std::mutex> lock(diagnosticRingbufferLock);

  if (diagnosticRingbuffer.size() > 64)
    diagnosticRingbuffer.pop_front();

  diagnosticRingbuffer.push_back(message);
}

bool load_saved_configuration() {
  Preferences prefs;
  prefs.begin("config", true);

  config.FirstTimeConfig = prefs.getBool("ftc", true);
  config.DevicePassword = prefs.getString("device_pw");
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

bool initialize_wifi_access_point() {
  log("Initializing WiFi connection...");

  if (requires_first_time_configuration()) {
    if (!WiFi.softAP(config.HostName.c_str(), config.WifiPassword.c_str())) {
      log("Unable to create WiFi Access point!");
      return false;
    }
  } else {
    if (!WiFi.begin(config.WifiSsid.c_str(), config.WifiPassword.c_str())) {
      log("Unable to create WiFi STA mode connection!");
      return false;
    }
  }

  while (WiFi.isConnected()) {
    log("Waiting 500ms for WiFi connection...");
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }


  WiFi.setAutoReconnect(true);

  log("WiFi connection established!");
  return true;
}

void http_configure() {
  if (http_show_login_if_required())
    return;

  String page{ FPSTR(ehal::PAGE_TEMPLATE) };
  page.replace(F("{{PAGE_SCRIPT}}"), FPSTR(ehal::SCRIPT_INJECT_AP_SSIDS));
  page.replace(F("{{PAGE_BODY}}"), FPSTR(ehal::BODY_TEMPLATE_CONFIG));

  page.replace(F("{{device_pw}}"), config.DevicePassword);
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

void http_save_configuration() {
  String page{ FPSTR(ehal::PAGE_TEMPLATE) };
  page.replace(F("{{PAGE_SCRIPT}}"), FPSTR(ehal::SCRIPT_WAIT_REBOOT));
  page.replace(F("{{PAGE_BODY}}"), FPSTR(ehal::BODY_TEMPLATE_CONFIG_SAVED));
  server.sendHeader("Connection", "close");
  server.send(200, F("text/html"), page);

  Preferences prefs;
  prefs.begin("config", /* readonly = */ false);
  prefs.putString("device_pw", server.arg("device_pw"));
  prefs.putString("wifi_ssid", server.arg("wifi_ssid"));
  prefs.putString("wifi_pw", server.arg("wifi_pw"));
  prefs.putString("hostname", server.arg("hostname"));
  prefs.putString("mqtt_server", server.arg("mqtt_server"));
  prefs.putUShort("mqtt_port", server.arg("mqtt_port").toInt());
  prefs.putString("mqtt_username", server.arg("mqtt_username"));
  prefs.putString("mqtt_pw", server.arg("mqtt_pw"));
  prefs.putString("mqtt_topic", server.arg("mqtt_topic"));
  prefs.putBool("ftc", false);
  prefs.end();

  ESP.restart();
}

void http_clear_config() {
  String page{ FPSTR(ehal::PAGE_TEMPLATE) };
  page.replace(F("{{PAGE_SCRIPT}}"), FPSTR(ehal::SCRIPT_WAIT_REBOOT));
  page.replace(F("{{PAGE_BODY}}"), FPSTR(ehal::BODY_TEMPLATE_CONFIG_CLEARED));
  server.sendHeader("Connection", "close");
  server.send(200, F("text/html"), page);

  Preferences prefs;
  prefs.begin("config", /* readonly = */ false);
  prefs.clear();
  prefs.end();

  ESP.restart();
}

void http_root() {
  if (http_show_login_if_required())
    return;

  String page{ FPSTR(ehal::PAGE_TEMPLATE) };
  page.replace(F("{{PAGE_SCRIPT}}"), "");
  page.replace(F("{{PAGE_BODY}}"), FPSTR(ehal::BODY_TEMPLATE_HOME));
  server.send(200, "text/html", page);
}

void http_query_ssid_list() {
  DynamicJsonDocument json(1024);
  JsonArray ssids = json.createNestedArray("ssids");

  int count = WiFi.scanNetworks();

  for (int i = 0; i < count; ++i) {
    ssids.add(WiFi.SSID(i));
  }

  String jsonOut;
  serializeJson(json, jsonOut);

  WiFi.scanDelete();

  server.send(200, F("text/plain"), jsonOut);
}

void http_query_life() {
  server.send(200, F("text/plain"), F("alive"));
}

void http_query_diagnostic_logs() {
  DynamicJsonDocument json(1024);
  JsonArray msg = json.createNestedArray("messages");

  {
    std::lock_guard<std::mutex> lock(diagnosticRingbufferLock);

    std::deque<String>::const_iterator end = std::end(diagnosticRingbuffer);
    for (std::deque<String>::const_iterator it = std::begin(diagnosticRingbuffer); it != end; ++it) {
      msg.add(*it);
    }
  }

  String jsonOut;
  serializeJson(json, jsonOut);

  server.send(200, F("text/plain"), jsonOut);
}

void http_diagnostics() {
  if (http_show_login_if_required())
    return;

  String page{ FPSTR(ehal::PAGE_TEMPLATE) };
  page.replace(F("{{PAGE_SCRIPT}}"), FPSTR(ehal::SCRIPT_UPDATE_DIAGNOSTIC_LOGS));
  page.replace(F("{{PAGE_BODY}}"), FPSTR(ehal::BODY_TEMPLATE_DIAGNOSTICS));

  char deviceMac[19] = {};
  snprintf(deviceMac, sizeof(deviceMac), "%#llx", ESP.getEfuseMac());

    page.replace(F("{{sw_ver}}"), F(SOFTWARE_VERSION));
    page.replace(F("{{device_mac}}"), deviceMac);
    page.replace(F("{{device_cpus}}"), String(ESP.getChipCores()));
    page.replace(F("{{device_cpu_freq}}"), String(ESP.getCpuFreqMHz())); 
    page.replace(F("{{device_free_heap}}"), String(ESP.getFreeHeap()));
    page.replace(F("{{device_total_heap}}"), String(ESP.getHeapSize()));
    page.replace(F("{{device_min_heap}}"), String(ESP.getMinFreeHeap()));   

    page.replace(F("{{wifi_hostname}}"), WiFi.getHostname());
    page.replace(F("{{wifi_ip}}"), WiFi.localIP().toString());
    page.replace(F("{{wifi_gateway_ip}}"), WiFi.gatewayIP().toString());
    page.replace(F("{{wifi_mac}}"), WiFi.macAddress());
    page.replace(F("{{wifi_mac}}"), WiFi.macAddress());
    page.replace(F("{{wifi_tx_power}}"), String(WiFi.getTxPower()));    

    server.send(200, F("text/html"), page);
}

void http_heat_pump() {
  if (http_show_login_if_required())
    return;

  String page{ FPSTR(ehal::PAGE_TEMPLATE) };
  page.replace(F("{{PAGE_SCRIPT}}"), "");
  page.replace(F("{{PAGE_BODY}}"), FPSTR(ehal::BODY_TEMPLATE_HEAT_PUMP));

  server.send(200, F("text/html"), page);
}

void http_firmware_update() {
  String page{ FPSTR(ehal::PAGE_TEMPLATE) };
  page.replace(F("{{PAGE_SCRIPT}}"), FPSTR(ehal::SCRIPT_WAIT_REBOOT));
  page.replace(F("{{PAGE_BODY}}"), FPSTR(ehal::BODY_TEMPLATE_FIRMWARE_UPDATE));

  server.sendHeader("Connection", "close");
  server.send(200, F("text/html"), page);
  ESP.restart();
}

void http_firmware_update_handler() {
  HTTPUpload& upload = server.upload();
  switch (upload.status) {
    case UPLOAD_FILE_START:
      {
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
          log(String("Failed to start firmware update: ") + Update.errorString());
        }
      }
      break;

    case UPLOAD_FILE_WRITE:
      {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
          log(String("Failed to write firmware chunk: ") + Update.errorString());
        }
      }
      break;

    case UPLOAD_FILE_END:
      {
        if (!Update.end(true)) {
          log(String("Failed to finalize firmware write: ") + Update.errorString());
        }
      }
      break;

    case UPLOAD_FILE_ABORTED:
      {
        log("Firmware update process aborted!");
      }
      break;
  }
}

String generate_login_cookie() {
  String payload = config.DevicePassword + String(xTaskGetTickCount());
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
  for (int i = 0; i < sizeof(sha256); ++i) {
    snprintf(hex, sizeof(hex), "%02x", sha256[i]);
    cookie += hex;
  }

  return cookie;
}

void http_redirect(const char* uri) {
  String page{ FPSTR(ehal::PAGE_TEMPLATE) };
  page.replace(F("{{PAGE_SCRIPT}}"), FPSTR(ehal::SCRIPT_REDIRECT));
  page.replace(F("{{PAGE_BODY}}"), FPSTR(ehal::BODY_TEMPLATE_REDIRECT));
  page.replace(F("{{uri}}"), uri);
  server.send(200, F("text/html"), page);
}

void http_verify_login() {
  if (server.arg("device_pw") == config.DevicePassword) {
    if (loginCookie.isEmpty()) {
      loginCookie = generate_login_cookie();
    }

    log("Successful device login, authorising client: " + loginCookie);

    server.sendHeader("Set-Cookie", String("login-cookie=") + loginCookie);
  } else {
    log("Device password mismatch! Login attempted with '" + server.arg("device_pw") + "'");
  }

  http_redirect("/");
}

bool http_show_login_if_required() {
  if (requires_first_time_configuration()) {
    log("Skipping login, as first time configuration is required");
    return false;
  }

  if (config.DevicePassword.isEmpty()) {
    log("Skipping login, as device password is unset.");
    return false;
  }

  if (!loginCookie.isEmpty()) {
    String clientCookie = server.header("Cookie");
    if (clientCookie.indexOf(String("login-cookie=") + loginCookie) != -1) {
      return false;
    }

    log("Client cookie mismatch, redirecting to login page");
  } else {
    log("Login cookie is unset, redirecting to login page");
  }

  String page{ FPSTR(ehal::PAGE_TEMPLATE) };
  page.replace(F("{{PAGE_BODY}}"), FPSTR(ehal::BODY_TEMPLATE_LOGIN));
  server.send(200, F("text/html"), page);
  return true;
}

bool requires_first_time_configuration() {
  return config.FirstTimeConfig;
}

void set_common_page_handlers() {
  // Common pages
  server.on("/diagnostics", http_diagnostics);
  server.on("/configuration", http_configure);
  server.on("/heat_pump", http_heat_pump);

  // Forms / magic URIs for buttons.
  server.on("/verify_login", http_verify_login);
  server.on("/save", http_save_configuration);
  server.on("/clear_config", http_clear_config);
  server.on("/update", HTTP_POST, http_firmware_update, http_firmware_update_handler);

  // Javascript XHTTPRequest
  server.on("/query_ssid", http_query_ssid_list);
  server.on("/query_life", http_query_life);
  server.on("/query_diagnostic_logs", http_query_diagnostic_logs);
}

bool initialize_captive_portal() {
  dnsServer.reset(new DNSServer());
  if (!dnsServer) {
    log("Failed to allocate DNS server!");
    return false;
  }

  if (!dnsServer->start(/*port =*/53, "*", WiFi.softAPIP())) {
    log("Failed to start DNS server!");
    return false;
  }

  log("Initialized DNS server for captive portal.");

  server.on("/", http_configure);
  server.onNotFound(http_configure);
  return true;
}

void setup() {
  if (!load_saved_configuration()) {
    log("Failed to load configuration!");
    return;
  }

  log("Configuration parameters loaded from NVS");

  initialize_wifi_access_point();

  if (requires_first_time_configuration()) {
    log("First time configuration required, starting captive portal...");

    initialize_captive_portal();
  } else {
    log("Regular startup mode, initializing web-server...");

    server.on("/", http_root);
    server.onNotFound(http_root);
  }

  set_common_page_handlers();

  const char* headers[] = { "Cookie" };
  server.collectHeaders(headers, sizeof(headers) / sizeof(char*));
  server.begin();

  log("Server startup successful, starting request processing.");
}

void loop() {
  if (dnsServer && requires_first_time_configuration()) {
    dnsServer->processNextRequest();
  }

  server.handleClient();
  delay(25);
}
