#include <cmath>
#include "ehal_diagnostics.h"
#include "ehal_thirdparty.h"
#include "esp_err.h"
#include "psram_alloc.h"
#include <chrono>

#include "time.h"

#include <cstdio>
#include <deque>
#include <memory>
#include <mutex>

#if ARDUINO_ARCH_ESP32
#include <esp_task_wdt.h>
#endif

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
        std::unique_ptr<char[]> buffer{ new char[MAX_MESSAGE_LENGTH] };

        va_list args;
        va_start(args, fmt);

        // Include timestamp in diagnostic log message.
        time_t now = time(nullptr);
        struct tm t = *localtime(&now);
        strftime(buffer.get(), MAX_MESSAGE_LENGTH, "[%T] ", &t);
        const size_t offset = 11; // "[14:55:02] "

        // Format remainder of log message into buffer.
        vsnprintf_P(buffer.get() + offset, MAX_MESSAGE_LENGTH - offset, (PGM_P)fmt, args);

        va_end(args);

        std::unique_lock<std::mutex> lock(diagnosticRingbufferLock, std::try_to_lock);

        auto start = std::chrono::steady_clock::now();
        while (!lock && (std::chrono::steady_clock::now() - start) < std::chrono::seconds(10))
        {
            lock.try_lock();
        }

        if (!lock)
            return;

        if (diagnosticRingbuffer.size() > MAX_NUM_ELEMENTS)
            diagnosticRingbuffer.pop_front();

        diagnosticRingbuffer.push_back(buffer.get());
    }

    void log_web_ratelimit(const __FlashStringHelper* fmt, ...)
    {
        static std::chrono::steady_clock::time_point last_log = std::chrono::steady_clock::now();

        std::chrono::steady_clock::time_point log_time = std::chrono::steady_clock::now();
        if (log_time - last_log > std::chrono::seconds(1))
        {
            last_log = log_time;
            std::unique_ptr<char[]> buffer{ new char[MAX_MESSAGE_LENGTH] };

            va_list args;
            va_start(args, fmt);

            // Include timestamp in diagnostic log message.
            time_t now = time(nullptr);
            struct tm t = *localtime(&now);
            strftime(buffer.get(), MAX_MESSAGE_LENGTH, "[%T] ", &t);
            const size_t offset = 11; // "[14:55:02] "

            // Format remainder of log message into buffer.
            vsnprintf_P(buffer.get() + offset, MAX_MESSAGE_LENGTH - offset, (PGM_P)fmt, args);

            va_end(args);

            std::unique_lock<std::mutex> lock(diagnosticRingbufferLock, std::try_to_lock);

            while (!lock && (std::chrono::steady_clock::now() - log_time) < std::chrono::seconds(10))
            {
                lock.try_lock();
            }

            if (!lock)
                return;

            if (diagnosticRingbuffer.size() > MAX_NUM_ELEMENTS)
                diagnosticRingbuffer.pop_front();

            diagnosticRingbuffer.push_back(buffer.get());
        }
    }

    String logs_as_json()
    {
        JsonDocument doc;        
        JsonArray msg = doc["messages"].to<JsonArray>();

        {
            std::lock_guard<std::mutex> lock(diagnosticRingbufferLock);

            psram::deque::const_iterator end = std::end(diagnosticRingbuffer);
            for (psram::deque::const_iterator it = std::begin(diagnosticRingbuffer); it != end; ++it)
            {
                msg.add(*it);
            }
        }

        String jsonOut;
        serializeJson(doc, jsonOut);

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

    void init_watchdog()
    {
#if ARDUINO_ARCH_ESP32
        esp_task_wdt_init(30, true); // Reset the board if the watchdog timer isn't reset every 30s.
        ehal::log_web(F("Watchdog initialized."));
#endif
    }

    void add_thread_to_watchdog()
    {
#if ARDUINO_ARCH_ESP32
        esp_task_wdt_add(nullptr);
#endif
    }

    void ping_watchdog()
    {
#if ARDUINO_ARCH_ESP32
        esp_task_wdt_reset();
#endif
    }
} // namespace ehal