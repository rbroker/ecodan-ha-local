#pragma once

#include <Arduino.h>

namespace ehal
{
    void log_web(const char* fmt, ...);
    String logs_as_json();
} // namespace ehal