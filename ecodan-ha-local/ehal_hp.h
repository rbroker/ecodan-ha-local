#pragma once

#include "Arduino.h"

namespace ehal::hp
{
    String entity_name();
    String get_device_model();

    float get_min_temperature();
    float get_max_temperature();
    float get_initial_temperature();
    float get_temperature_step();

    float get_current_temperature();
    String get_current_mode();
    String get_current_action();

    String get_mode_status_template();
    String get_temperature_status_template();
    String get_current_temperature_status_template();    

    bool initialize();
    void handle_loop();
}