#pragma once

#include <Arduino.h>

namespace ehal
{
    void log_web(const __FlashStringHelper* fmt, ...);
    void log_web_ratelimit(const __FlashStringHelper* fmt, ...);

    float get_cpu_temperature();

    String logs_as_json();
} // namespace ehal