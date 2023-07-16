#include "esp_err.h"
#include "ehal_diagnostics.h"
#include "ehal_thirdparty.h"
#include "psram_alloc.h"

#include "time.h"

#include <cstdio>
#include <deque>
#include <mutex>

#if CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32C3
#include <driver/temp_sensor.h>
#endif

namespace ehal
{
    std::mutex diagnosticRingbufferLock;
    psram::deque diagnosticRingbuffer;

#define MAX_MESSAGE_LENGTH 255U
#define MAX_NUM_ELEMENTS 32U    

    void log_web(const __FlashStringHelper* fmt, ...)
    {
        char buffer[MAX_MESSAGE_LENGTH] = {};

        va_list args;
        va_start(args, fmt);

        // Include timestamp in diagnostic log message.
        time_t now = time(nullptr);
        struct tm t = *localtime(&now);
        strftime(buffer, sizeof(buffer), "[%T] ", &t);
        const size_t offset = 11; // "[14:55:02] "

        // Format remainder of log message into buffer.
        vsnprintf_P(buffer + offset, sizeof(buffer) - offset, (PGM_P)fmt, args);

        va_end(args);

        std::lock_guard<std::mutex> lock(diagnosticRingbufferLock);

        if (diagnosticRingbuffer.size() > MAX_NUM_ELEMENTS)
            diagnosticRingbuffer.pop_front();

        diagnosticRingbuffer.push_back(buffer);
    }

    void log_web_ratelimit(const __FlashStringHelper* fmt, ...)
    {
        static std::chrono::steady_clock::time_point last_log = std::chrono::steady_clock::now();

        std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
        if (now - last_log > std::chrono::seconds(1))
        {
            last_log = now;
            char buffer[MAX_MESSAGE_LENGTH] = {};

            va_list args;
            va_start(args, fmt);

            // Include timestamp in diagnostic log message.
            time_t now = time(nullptr);
            struct tm t = *localtime(&now);
            strftime(buffer, sizeof(buffer), "[%T] ", &t);
            const size_t offset = 11; // "[14:55:02] "

            // Format remainder of log message into buffer.
            vsnprintf_P(buffer + offset, sizeof(buffer) - offset, (PGM_P)fmt, args);

            va_end(args);

            std::lock_guard<std::mutex> lock(diagnosticRingbufferLock);

            if (diagnosticRingbuffer.size() > MAX_NUM_ELEMENTS)
                diagnosticRingbuffer.pop_front();

            diagnosticRingbuffer.push_back(buffer);
        }
    }

    String logs_as_json()
    {
        DynamicJsonDocument json((MAX_MESSAGE_LENGTH * MAX_NUM_ELEMENTS) + 1024);
        JsonArray msg = json.createNestedArray(F("messages"));

        {
            std::lock_guard<std::mutex> lock(diagnosticRingbufferLock);

            psram::deque::const_iterator end = std::end(diagnosticRingbuffer);
            for (psram::deque::const_iterator it = std::begin(diagnosticRingbuffer); it != end; ++it)
            {
                msg.add(*it);
            }
        }

        String jsonOut;
        serializeJson(json, jsonOut);

        return jsonOut;
    }
    
    float get_cpu_temperature()
    {
#if CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32C3
        static bool started = false;

        if (!started)
        {
            temp_sensor_start();
            started = true;            
        }

        float temp;
        temp_sensor_read_celsius(&temp);        

        return temp;
#else
        return 0.0f;
#endif
    }

} // namespace ehal