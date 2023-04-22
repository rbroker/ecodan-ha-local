#pragma once

namespace ehal
{
    const char* PAGE_TEMPLATE PROGMEM = R"(<!DOCTYPE html><html lang="en">
    <head>
        <title>Ecodan: HomeAssistant Bridge</title>
        <meta charset="utf-8" />
        <meta name="viewport" content="width=device-width, initial-scale=1" />
        <link rel="stylesheet" href="/milligram.css">        
        <script type="text/javascript" {{PAGE_SCRIPT}}></script>
    </head>
    <body class="container">
        {{PAGE_BODY}}
    </body>
</html>)";

    const char* BODY_TEMPLATE_HOME PROGMEM = R"(<h1>Home</h1>
<nav class="row">
    <a class="button button-outline column column-25" href="/">Home</a>
    <a class="button button-clear column column-25" href="/configuration">Configuration</a>
    <a class="button button-clear column column-25" href="/diagnostics">Diagnostics</a>
    <a class="button button-clear column column-25" href="/heat_pump">Heat Pump</a>
</nav>
<br />)";

    const char* BODY_TEMPLATE_CONFIG PROGMEM = R"(<h1>Configuration</h1>
<nav class="row">
    <a class="button button-clear column column-25" href="/">Home</a>
    <a class="button button-outline column column-25" href="/configuration">Configuration</a>
    <a class="button button-clear column column-25" href="/diagnostics">Diagnostics</a>
    <a class="button button-clear column column-25" href="/heat_pump">Heat Pump</a>
</nav>
<br />
<form method="post" action="save">
    <h2>Device:</h2>
    <div class="row">
        <label class="column column-25" for="device_pw">Device Password:</label>
        <input class="column column-75" type="password" id="device_pw" max-length="100" name="device_pw" value="{{device_pw}}" />
    </div>
    <div class="row">
        <label class="column column-25" for="device_tz">Time Zone:</label>
        <input class="column column-75" type="text" id="device_tz" name="device_tz" value="{{device_tz}}" />
    </div>
    <br />
    <h2>WiFi Configuration:</h2>
    <div class="row">
        <label  class="column column-25" for="wifi_ssid">WiFi SSID:</label>
        <select class="column column-75" id="wifi_ssid" name="wifi_ssid" required>
            <option id="pre-ssid" value="{{wifi_ssid}}">{{wifi_ssid}}</option>
        </select>
    </div>
    <div class="row">
        <label class="column column-25" for="wifi_pw">WiFi Password:</label>
        <input class="column column-75" type="password" id="wifi_pw" name="wifi_pw" minlength="15" value="{{wifi_pw}}" required />
    </div>
    <div class="row">
        <label class="column column-25" for="hostname">Hostname:</label>
        <input class="column column-75" type="text" id="hostname" name="hostname" value="{{hostname}}" />
    </div>
    <br />
    <div class="row">
        <input class="column column-25 column-offset-75" id='reload' type="button" value="Reload Wifi SSIDs" onclick='inject_ssid_list()' />
    </div>
    <br />
    <h2>MQTT Configuration:</h2>
    <div class="row">
        <label class="column column-25" for="mqtt_server">MQTT Server:</label>
        <input class="column column-75" type="text" id="mqtt_server" name="mqtt_server" value="{{mqtt_server}}" required />
    </div>
    <div class="row">
        <label class="column column-25" for="mqtt_port">MQTT Port:</label>
        <input class="column column-75" type="text" inputmode="numeric" id="mqtt_port" name="mqtt_port" value="{{mqtt_port}}" required />
    </div>
    <div class="row">
        <label class="column column-25" for="mqtt_user">MQTT User:</label>
        <input class="column column-75" type="text" id="mqtt_user" name="mqtt_user" value="{{mqtt_user}}" required />
    </div>
    <div class="row">
        <label class="column column-25" for="mqtt_pw">MQTT Password:</label>
        <input class="column column-75" type="password" id="mqtt_pw" name="mqtt_pw" value="{{mqtt_pw}}" required />
    </div>
    <div class="row">
        <label class="column column-25" for="mqtt_topic">MQTT Topic:</label>
        <input class="column column-75" type="text" id="mqtt_topic" name="mqtt_topic" value="{{mqtt_topic}}" required />
    </div>
    <br />
    <div class="row">
        <input class="column column-25" id="reset" type="button" value="Restore Defaults" onclick='clear_config()' />
        <input class="button column column-25 column-offset-50" id="save" type="submit" value="Save & Reboot" />
    </div>
</form>
<br />
<form method="POST" id="update_form" action="update" enctype="multipart/form-data">
    <h2>Firmware Update</h2>
    <div class="row">
        <label class="column column-25" for="update">Firmware Binary:</label>
        <input class="column column-25" type="file" name="update" />
        <input class="column column-25 column-offset-25" id="update" type="button" value="Update" onclick='trigger_update()' />
    </div>
</form>
)";

    const char* BODY_TEMPLATE_CONFIG_SAVED PROGMEM = R"(<p>Configuration Saved! Rebooting...<span id="reboot_progress"><span></p>)";

    const char* BODY_TEMPLATE_CONFIG_CLEARED PROGMEM = R"(<p>Configuration Reset To Default! Rebooting...<span id="reboot_progress"></span></p>)";

    const char* BODY_TEMPLATE_FIRMWARE_UPDATE PROGMEM = R"(<p>Updating firmware...<span id="reboot_progress"></span></p>)";

    const char* BODY_TEMPLATE_REDIRECT PROGMEM = R"(<p>Redirecting...</p>)";

    const char* BODY_TEMPLATE_DIAGNOSTICS PROGMEM = R"(<h1>Diagnostics</h1>
<nav class="row">
    <a class="button button-clear column column-25" href="/">Home</a>
    <a class="button button-clear column column-25" href="/configuration">Configuration</a>
    <a class="button button-outline column column-25" href="/diagnostics">Diagnostics</a>
    <a class="button button-clear column column-25" href="/heat_pump">Heat Pump</a>
</nav>
<h2>Device Info</h2>
<table>
    <tr>
        <td>Software Version:</td>
        <td>{{sw_ver}}</td>
    </tr>
    <tr>
        <td>Device MAC:</td>
        <td>{{device_mac}}</td>
    </tr>
    <tr>
        <td>Device CPU Cores:</td>
        <td>{{device_cpus}}</td>
    </tr>
    <tr>
        <td>Device CPU Frequency:</td>
        <td>{{device_cpu_freq}}Mhz</td>
    </tr>
    <tr>
        <td>Device Free Heap:</td>
        <td>{{device_free_heap}}</td>
    </tr>
    <tr>
        <td>Device Total Heap:</td>
        <td>{{device_total_heap}}</td>
    </tr>
    <tr>
        <td>Device Heap Low Watermark:</td>
        <td>{{device_min_heap}}</td>
    </tr>
    <tr>
        <td>Device Free PSRAM:</td>
        <td>{{device_free_psram}}</td>
    </tr>
    <tr>
        <td>Device Total PSRAM:</td>
        <td>{{device_total_psram}}</td>
    </tr>
    <tr>
        <td>WiFi HostName:</td>
        <td>{{wifi_hostname}}</td>
    </tr>
    <tr>
        <td>WiFi IP:</td>
        <td>{{wifi_ip}}</td>
    <tr>
        <td>WiFi Gateway IP:</td>
        <td>{{wifi_gateway_ip}}</td>
    </tr>
    <tr>
        <td>WiFi MAC:</td>
        <td>{{wifi_mac}}</td>
    </tr>
    <tr>
        <td>WiFi Tx Power:</td>
        <td>{{wifi_tx_power}}</td>
    </tr>
    <tr>
        <td>HomeAssistant Heat Pump Entity:</td>
        <td>{{ha_hp_entity}}</td>
    </tr>
</table>
<h2>Logs</h2>
<pre><code class="column column-33 column-offset-33" style="max-height:250px;overflow:auto;" id="logs">
</code></pre>)";

    const char* BODY_TEMPLATE_HEAT_PUMP PROGMEM = R"(<h1>Heat Pump</h1>
<nav class="row">
    <a class="button button-clear column column-25" href="/">Home</a>
    <a class="button button-clear column column-25" href="/configuration">Configuration</a>
    <a class="button button-clear column column-25" href="/diagnostics">Diagnostics</a>
    <a class="button button-outline column column-25" href="/heat_pump">Heat Pump</a>
</nav>)";

    const char* BODY_TEMPLATE_LOGIN PROGMEM = R"(<h1>Login</h1>
<form method="post" action="verify_login">
    <div class="row">
        <label class="column column-25" for="device_pw">Device Password:</label>
        <input class="column column-75" type="password" id="device_pw" name="device_pw" value="" />
    </div>
    <br />
    <div class="row">
        <input class="button column column-25 column-offset-75" type="submit" value="Login" class="button" />
    </div>
</form>
)";
} // namespace ehal