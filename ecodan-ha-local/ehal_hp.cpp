#include "ehal.h"
#include "ehal_diagnostics.h"
#include "ehal_hp.h"

namespace ehal::hp
{
    String entity_name()
    {
        return String("ecodan_hp_") + device_mac();
    }

    String get_device_model()
    {
        return F("Ecodan PUZ-WM60VAA");
    }

    float get_min_temperature()
    {
        return 14.0f;
    }

    float get_max_temperature()
    {
        return 32.0f;
    }

    float get_initial_temperature()
    {
        return 21.0f;
    }

    float get_temperature_step()
    {
        return 0.5f;
    }

    float get_current_temperature()
    {
        return 20.0f;
    }

    String get_current_mode()
    {
        return F("off");
    }

    String get_current_action()
    {
        // { off, heating, cooling, drying, idle, fan }
        return F("idle");
    }

    String get_mode_status_template()
    {
        String tpl(F(R"(
{% if (value_json is defined and value_json.mode is defined) %}
{{ value_json.mode }}
{% else %}
off
{% endif %})"));

        tpl.replace(F("\n"), "");
        tpl.trim();
        return tpl;
    }

    String get_temperature_status_template()
    {
        String tpl(F(R"(
{% if (value_json is defined and value_json.temperature is defined) %}
{% if (value_json.temperature|int >= {{min_temp}} and value_json.temperature|int <= {{max_temp}}) %}
{{ value_json.temperature }}
{% elif (value_json.temperature|int < {{min_temp}}) %}
{{min_temp}}
{% elif (value_json.temperature|int > {{max_temp}}) %}
{{max_temp}}
{% endif %}
{% else %}
21
{% endif %})"));

        tpl.replace(F("\n"), "");
        tpl.replace(F("{{min_temp}}"), String(get_min_temperature()));
        tpl.replace(F("{{max_temp}}"), String(get_max_temperature()));
        tpl.trim();

        return tpl;
    }

    String get_current_temperature_status_template()
    {
        String tpl(F(R"(
{% if (value_json is defined and value_json.curr_temp is defined) %}
{{ value_json.curr_temp }}
{% else %}
0
{% endif %})"));

        tpl.replace(F("\n"), "");
        tpl.trim();

        return tpl;
    }

    bool initialize()
    {
        log_web("Initializing HeatPump...");
        return true;
    }

    void handle_loop()
    {
    }
} // namespace ehal::hp