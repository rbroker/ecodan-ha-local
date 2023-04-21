#include "ehal_diagnostics.h"
#include "ehal_thirdparty.h"

#include <cstdio>
#include <deque>
#include <mutex>

namespace ehal
{
    std::mutex diagnosticRingbufferLock;
    std::deque<String> diagnosticRingbuffer;

#define MAX_MESSAGE_LENGTH 255U
#define MAX_NUM_ELEMENTS 32U

    void log_web(const char* fmt, ...)
    {
        char buffer[MAX_MESSAGE_LENGTH] = {};

        va_list args;
        va_start(args, fmt);

        vsnprintf(buffer, sizeof(buffer), fmt, args);

        va_end(args);

        std::lock_guard<std::mutex> lock(diagnosticRingbufferLock);

        if (diagnosticRingbuffer.size() > MAX_NUM_ELEMENTS)
            diagnosticRingbuffer.pop_front();

        diagnosticRingbuffer.push_back(buffer);
    }

    String logs_as_json()
    {
        DynamicJsonDocument json((MAX_MESSAGE_LENGTH * MAX_NUM_ELEMENTS) + 1024);
        JsonArray msg = json.createNestedArray("messages");

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