#include "ehal_diagnostics.h"
#include "ehal_thirdparty.h"

#include "time.h"

#include <cstdio>
#include <deque>
#include <mutex>

namespace ehal
{
    std::mutex diagnosticRingbufferLock;
    std::deque<String> diagnosticRingbuffer;

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

            std::deque<String>::const_iterator end = std::end(diagnosticRingbuffer);
            for (std::deque<String>::const_iterator it = std::begin(diagnosticRingbuffer); it != end; ++it)
            {
                msg.add(*it);
            }
        }

        String jsonOut;
        serializeJson(json, jsonOut);

        return jsonOut;
    }
} // namespace ehal