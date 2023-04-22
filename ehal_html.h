#pragma once

namespace ehal
{
    const char* PAGE_TEMPLATE PROGMEM = R"(<!DOCTYPE html><html lang="en">
    <head>
        <title>Ecodan: HomeAssistant Bridge</title>
        <meta charset="utf-8" />
        <meta name="viewport" content="width=device-width, initial-scale=1" />
        <style>*,*:after,*:before{box-sizing:inherit}html{box-sizing:border-box;font-size:62.5%}body{color:#606c76;font-family:'Roboto','Helvetica Neue','Helvetica','Arial',sans-serif;font-size:1.6em;font-weight:300;letter-spacing:.01em;line-height:1.6}blockquote{border-left:.3rem solid #d1d1d1;margin-left:0;margin-right:0;padding:1rem 1.5rem}blockquote *:last-child{margin-bottom:0}.button,button,input[type='button'],input[type='reset'],input[type='submit']{background-color:#9b4dca;border:.1rem solid #9b4dca;border-radius:.4rem;color:#fff;cursor:pointer;display:inline-block;font-size:1.1rem;font-weight:700;height:3.8rem;letter-spacing:.1rem;line-height:3.8rem;padding:0 3rem;text-align:center;text-decoration:none;text-transform:uppercase;white-space:nowrap}.button:focus,.button:hover,button:focus,button:hover,input[type='button']:focus,input[type='button']:hover,input[type='reset']:focus,input[type='reset']:hover,input[type='submit']:focus,input[type='submit']:hover{background-color:#606c76;border-color:#606c76;color:#fff;outline:0}.button[disabled],button[disabled],input[type='button'][disabled],input[type='reset'][disabled],input[type='submit'][disabled]{cursor:default;opacity:.5}.button[disabled]:focus,.button[disabled]:hover,button[disabled]:focus,button[disabled]:hover,input[type='button'][disabled]:focus,input[type='button'][disabled]:hover,input[type='reset'][disabled]:focus,input[type='reset'][disabled]:hover,input[type='submit'][disabled]:focus,input[type='submit'][disabled]:hover{background-color:#9b4dca;border-color:#9b4dca}.button.button-outline,button.button-outline,input[type='button'].button-outline,input[type='reset'].button-outline,input[type='submit'].button-outline{background-color:transparent;color:#9b4dca}.button.button-outline:focus,.button.button-outline:hover,button.button-outline:focus,button.button-outline:hover,input[type='button'].button-outline:focus,input[type='button'].button-outline:hover,input[type='reset'].button-outline:focus,input[type='reset'].button-outline:hover,input[type='submit'].button-outline:focus,input[type='submit'].button-outline:hover{background-color:transparent;border-color:#606c76;color:#606c76}.button.button-outline[disabled]:focus,.button.button-outline[disabled]:hover,button.button-outline[disabled]:focus,button.button-outline[disabled]:hover,input[type='button'].button-outline[disabled]:focus,input[type='button'].button-outline[disabled]:hover,input[type='reset'].button-outline[disabled]:focus,input[type='reset'].button-outline[disabled]:hover,input[type='submit'].button-outline[disabled]:focus,input[type='submit'].button-outline[disabled]:hover{border-color:inherit;color:#9b4dca}.button.button-clear,button.button-clear,input[type='button'].button-clear,input[type='reset'].button-clear,input[type='submit'].button-clear{background-color:transparent;border-color:transparent;color:#9b4dca}.button.button-clear:focus,.button.button-clear:hover,button.button-clear:focus,button.button-clear:hover,input[type='button'].button-clear:focus,input[type='button'].button-clear:hover,input[type='reset'].button-clear:focus,input[type='reset'].button-clear:hover,input[type='submit'].button-clear:focus,input[type='submit'].button-clear:hover{background-color:transparent;border-color:transparent;color:#606c76}.button.button-clear[disabled]:focus,.button.button-clear[disabled]:hover,button.button-clear[disabled]:focus,button.button-clear[disabled]:hover,input[type='button'].button-clear[disabled]:focus,input[type='button'].button-clear[disabled]:hover,input[type='reset'].button-clear[disabled]:focus,input[type='reset'].button-clear[disabled]:hover,input[type='submit'].button-clear[disabled]:focus,input[type='submit'].button-clear[disabled]:hover{color:#9b4dca}code{background:#f4f5f6;border-radius:.4rem;font-size:86%;margin:0 .2rem;padding:.2rem .5rem;white-space:nowrap}pre{background:#f4f5f6;border-left:.3rem solid #9b4dca;overflow-y:hidden}pre>code{border-radius:0;display:block;padding:1rem 1.5rem;white-space:pre}hr{border:0;border-top:.1rem solid #f4f5f6;margin:3rem 0}input[type='color'],input[type='date'],input[type='datetime'],input[type='datetime-local'],input[type='email'],input[type='month'],input[type='number'],input[type='password'],input[type='search'],input[type='tel'],input[type='text'],input[type='url'],input[type='week'],input:not([type]),textarea,select{-webkit-appearance:none;background-color:transparent;border:.1rem solid #d1d1d1;border-radius:.4rem;box-shadow:none;box-sizing:inherit;height:3.8rem;padding:.6rem 1rem .7rem;width:100%}input[type='color']:focus,input[type='date']:focus,input[type='datetime']:focus,input[type='datetime-local']:focus,input[type='email']:focus,input[type='month']:focus,input[type='number']:focus,input[type='password']:focus,input[type='search']:focus,input[type='tel']:focus,input[type='text']:focus,input[type='url']:focus,input[type='week']:focus,input:not([type]):focus,textarea:focus,select:focus{border-color:#9b4dca;outline:0}select{background:url('data:image/svg+xml;utf8,<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 30 8" width="30"><path fill="%23d1d1d1" d="M0,0l6,8l6-8"/></svg>') center right no-repeat;padding-right:3rem}select:focus{background-image:url('data:image/svg+xml;utf8,<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 30 8" width="30"><path fill="%239b4dca" d="M0,0l6,8l6-8"/></svg>')}select[multiple]{background:none;height:auto}textarea{min-height:6.5rem}label,legend{display:block;font-size:1.6rem;font-weight:700;margin-bottom:.5rem}fieldset{border-width:0;padding:0}input[type='checkbox'],input[type='radio']{display:inline}.label-inline{display:inline-block;font-weight:400;margin-left:.5rem}.container{margin:0 auto;max-width:112rem;padding:0 2rem;position:relative;width:100%}.row{display:flex;flex-direction:column;padding:0;width:100%}.row.row-no-padding{padding:0}.row.row-no-padding>.column{padding:0}.row.row-wrap{flex-wrap:wrap}.row.row-top{align-items:flex-start}.row.row-bottom{align-items:flex-end}.row.row-center{align-items:center}.row.row-stretch{align-items:stretch}.row.row-baseline{align-items:baseline}.row .column{display:block;flex:1 1 auto;margin-left:0;max-width:100%;width:100%}.row .column.column-10{flex:0 0 10%;max-width:10%}.row .column.column-20{flex:0 0 20%;max-width:20%}.row .column.column-25{flex:0 0 25%;max-width:25%}.row .column.column-33,.row .column.column-34{flex:0 0 33.3333%;max-width:33.3333%}.row .column.column-40{flex:0 0 40%;max-width:40%}.row .column.column-50{flex:0 0 50%;max-width:50%}.row .column.column-60{flex:0 0 60%;max-width:60%}.row .column.column-66,.row .column.column-67{flex:0 0 66.6666%;max-width:66.6666%}.row .column.column-75{flex:0 0 75%;max-width:75%}.row .column.column-80{flex:0 0 80%;max-width:80%}.row .column.column-90{flex:0 0 90%;max-width:90%}.row .column .column-top{align-self:flex-start}.row .column .column-bottom{align-self:flex-end}.row .column .column-center{align-self:center}.row .column[class*=" column-"]{margin-left:0;max-width:100%}@media (min-width:40rem){.row{flex-direction:row;margin-left:-1rem;width:calc(100% + 2.0rem)}.row .column{margin-bottom:inherit;padding:0 1rem}.row .column.column-offset-10{margin-left:10%}.row .column.column-offset-20{margin-left:20%}.row .column.column-offset-25{margin-left:25%}.row .column.column-offset-33,.row .column.column-offset-34{margin-left:33.3333%}.row .column.column-offset-40{margin-left:40%}.row .column.column-offset-50{margin-left:50%}.row .column.column-offset-60{margin-left:60%}.row .column.column-offset-66,.row .column.column-offset-67{margin-left:66.6666%}.row .column.column-offset-75{margin-left:75%}.row .column.column-offset-80{margin-left:80%}.row .column.column-offset-90{margin-left:90%}}a{color:#9b4dca;text-decoration:none}a:focus,a:hover{color:#606c76}dl,ol,ul{list-style:none;margin-top:0;padding-left:0}dl dl,dl ol,dl ul,ol dl,ol ol,ol ul,ul dl,ul ol,ul ul{font-size:90%;margin:1.5rem 0 1.5rem 3rem}ol{list-style:decimal inside}ul{list-style:circle inside}.button,button,dd,dt,li{margin-bottom:1rem}fieldset,input,select,textarea{margin-bottom:1.5rem}blockquote,dl,figure,form,ol,p,pre,table,ul{margin-bottom:2.5rem}table{border-spacing:0;display:block;overflow-x:auto;text-align:left;width:100%}td,th{border-bottom:.1rem solid #e1e1e1;padding:1.2rem 1.5rem}td:first-child,th:first-child{padding-left:0}td:last-child,th:last-child{padding-right:0}@media (min-width:40rem){table{display:table;overflow-x:initial}}b,strong{font-weight:700}p{margin-top:0}h1,h2,h3,h4,h5,h6{font-weight:300;letter-spacing:-.1rem;margin-bottom:2rem;margin-top:0}h1{font-size:4.6rem;line-height:1.2}h2{font-size:3.6rem;line-height:1.25}h3{font-size:2.8rem;line-height:1.3}h4{font-size:2.2rem;letter-spacing:-.08rem;line-height:1.35}h5{font-size:1.8rem;letter-spacing:-.05rem;line-height:1.5}h6{font-size:1.6rem;letter-spacing:0;line-height:1.4}img{max-width:100%}.clearfix:after{clear:both;content:' ';display:table}.float-left{float:left}.float-right{float:right}</style>
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