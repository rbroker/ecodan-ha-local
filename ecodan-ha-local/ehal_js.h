#pragma once

namespace ehal
{
    /*
     * Javascript snippet which provides a way to popuplate a "select" element with WiFi access point IDs returned
     * by a network scan, without blocking page load. If any of the returned SSIDs match the SSID we've previously
     * saved in our preferences, then attempt to select it automatically.
     */
    const char* SCRIPT_CONFIGURATION_PAGE PROGMEM = R"(function update_ssi() {
        let selected = document.getElementById('wifi_ssid');
        for (let i = 0; i < wifi.length; ++i) {
            if (wifi[i].ssid != selected.value)
                continue;

            let el = document.getElementById('ssi');
            el.className = 'signal-icon';
            if (wifi[i].rssi > -30) {
                el.classList.add('v');
            } else if (wifi[i].rssi > -55) {
                el.classList.add('iv');
            } else if (wifi[i].rssi > -67) {
                el.classList.add('iii');
            } else if (wifi[i].rssi > -70) {
                el.classList.add('ii');
            } else if (wifi[i].rssi > -80) {
                el.classList.add('i');
            }
        }
    }

    function inject_ssid_list() {
        disable_all_inputs();
        let select = document.getElementById('wifi_ssid');
        let option = document.createElement('option');
        select.innerHTML = '';
        option.value = '';
        option.textContent = 'Loading SSIDs...';
        select.disabled = true;
        select.appendChild(option);

        let xhttp = new XMLHttpRequest();
        xhttp.open('GET', 'query_ssid', true);
        xhttp.onload = function() {
            let select = document.getElementById('wifi_ssid');
            if (this.status == 200) {
                select.disabled = false;
                select.innerHTML = '';
                let jsonResponse = JSON.parse(this.responseText);
                wifi = jsonResponse.wifi;
                for (let i = 0; i < jsonResponse.wifi.length; ++i) {
                    let option = document.createElement('option');
                    option.value = jsonResponse.wifi[i].ssid;
                    option.textContent = jsonResponse.wifi[i].ssid;
                    if (jsonResponse.wifi[i].ssid == '{{wifi_ssid}}') {
                        option.selected = true;
                    }
                    select.appendChild(option);
                }
                update_ssi();
            } else if (this.status == 202) {
                window.setTimeout(inject_ssid_list, 500);
                return;
            } else {
                select.disabled = true;
            }

            enable_all_inputs();
        }
        xhttp.error = enable_all_inputs;
        xhttp.timeout = enable_all_inputs;
        xhttp.send();
    }

    function clear_config() {
        if (window.confirm("This will reset all configuration settings to their default values and reboot the device. Are you sure?")) {
            disable_all_inputs();
            window.location = '/clear_config';
        }
    }

    function toggle_inputs(state) {
        document.getElementById('reload').disabled = state;
        document.getElementById('save').disabled = state;
        document.getElementById('reset').disabled = state;
        document.getElementById('update').disabled = state;
    }

    function enable_all_inputs() {
        toggle_inputs(false);
    }

    function disable_all_inputs() {
        toggle_inputs(true);
    }

    function initial_ssid_query() {
        let option = document.getElementById('pre-ssid');
        if (!option || !option.value || option.value != '{{wifi_ssid}}') {
            inject_ssid_list();
        }
    }

    function on_page_load() {
        let form = document.getElementById('update_form');

        form.addEventListener('submit', function(e) {
            e.preventDefault();
            disable_all_inputs();

            var xhr = new XMLHttpRequest();
            xhr.upload.addEventListener('progress', function(e) {
                if (!e.lengthComputable)
                    return;

                let el = document.getElementById('update');
                el.value = Math.round((e.loaded / e.total) * 100) + '%';
            }, false);
            xhr.upload.addEventListener('load', function(event) {
                let el = document.getElementById('update');
                el.value = "Done!";
            }, false);
            xhr.addEventListener('readystatechange', function(event) {
                if (event.target.readyState == 4 && event.target.responseText) {
                    var doc = document.open('text/html', 'replace');
                    doc.write(event.target.responseText);
                    doc.close();
                }
            }, false);

            xhr.open(this.getAttribute('method'), this.getAttribute('action'), true);
            xhr.send(new FormData(this));
        });

        initial_ssid_query();
    }

    window.addEventListener('load', on_page_load);)";

    const char* SCRIPT_WAIT_REBOOT PROGMEM = R"(function check_alive() {
        let xhttp = new XMLHttpRequest();
        xhttp.open('GET', 'query_life', true);
        xhttp.timeout = 1000;
        xhttp.onload = function() {
            if (this.status == 200) {
                window.location = '/';
            } else {
                update_reboot_progress();
            }
        }
        xhttp.error = update_reboot_progress;
        xhttp.timeout = update_reboot_progress;
        xhttp.send();
    }

    function update_reboot_progress() {
         let rebootProgress = document.getElementById('reboot_progress');
        rebootProgress.textContent += '.';
        check_alive();
    }

    function trigger_life_check() {
        window.setTimeout(check_alive, 2000);
    }

    let wifi = [];
    window.addEventListener('load', trigger_life_check))";

    const char* SCRIPT_UPDATE_DIAGNOSTIC_LOGS PROGMEM = R"(function update_diagnostic_logs() {
        let xhttp = new XMLHttpRequest();
        xhttp.open('GET', 'query_diagnostic_logs', true);
        xhttp.onload = function() {
            let element = document.getElementById('logs');
            if (this.status == 200) {
                let didLogsChange = false;
                let jsonResponse = JSON.parse(this.responseText);
                for (let i = 0; i < jsonResponse.messages.length; ++i) {
                    if (element.textContent.indexOf(jsonResponse.messages[i]) == -1) {
                        element.textContent += jsonResponse.messages[i] + '\n';
                        didLogsChange = true;
                    }
                }
                if (didLogsChange) {
                    element.scrollTop = element.scrollHeight;
                }
            }
        }
        xhttp.send();
    }

    window.setInterval(update_diagnostic_logs, 5000);
    window.addEventListener('load', update_diagnostic_logs);)";

    const char* SCRIPT_REDIRECT PROGMEM = R"(window.location = '{{uri}}';
    )";
} // namespace ehal