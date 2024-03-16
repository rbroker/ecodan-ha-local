#include "ehal.h"
#include "ehal_config.h"
#include "ehal_diagnostics.h"
#include "ehal_hp.h"
#include "ehal_proto.h"

#include <HardwareSerial.h>
#include <freertos/task.h>

#include <algorithm>
#include <condition_variable>
#include <mutex>
#include <deque>
#include <thread>

namespace ehal::hp
{
    HardwareSerial port = Serial1;
    uint64_t rxMsgCount = 0;
    uint64_t txMsgCount = 0;

    TaskHandle_t serialRxTaskHandle = nullptr;
    std::thread serialRxThread;
    std::thread serialTxThread;
    std::deque<Message> cmdQueue;
    std::mutex cmdQueueMutex;
    std::condition_variable cmdQueueCv;

    Status status;
    float temperatureStep = 0.5f;
    bool connected = false;

    bool serial_tx(Message& msg)
    {
        if (!port)
        {
            log_web_ratelimit(F("Serial connection unavailable for tx"));
            return false;
        }

        msg.set_checksum();
        port.write(msg.buffer(), msg.size());

        auto& config = config_instance();
        if (config.DumpPackets)
        {
            msg.debug_dump_packet();
        }

        ++txMsgCount;
        return true;
    }

    void clear_command_queue()
    {
        std::lock_guard<std::mutex> lock{cmdQueueMutex};

        while (!cmdQueue.empty())
            cmdQueue.pop_front();
    }

    void resync_rx()
    {
        while (port.available() > 0)
            port.read();

        clear_command_queue();
    }

    bool serial_rx(Message& msg)
    {
        if (!port)
        {
            log_web_ratelimit(F("Serial connection unavailable for rx"));
            return false;
        }

        if (port.available() < HEADER_SIZE)
        {
            const TickType_t maxBlockingTime = pdMS_TO_TICKS(1000);
            ulTaskNotifyTakeIndexed(0, pdTRUE, maxBlockingTime);

            // We were woken by an interrupt, but there's not enough data available
            // yet on the serial port for us to start processing it as a packet.
            if (port.available() < HEADER_SIZE)
                return false;
        }

        // Scan for the start of an Ecodan packet.
        if (port.peek() != HEADER_MAGIC_A)
        {
            log_web_ratelimit(F("Dropping serial data, header magic mismatch"));
            resync_rx();
            return false;
        }

        if (port.readBytes(msg.buffer(), HEADER_SIZE) < HEADER_SIZE)
        {
            log_web(F("Serial port header read failure!"));
            resync_rx();
            return false;
        }

        msg.increment_write_offset(HEADER_SIZE);

        if (!msg.verify_header())
        {
            log_web(F("Serial port message appears invalid, skipping payload wait..."));
            resync_rx();
            return false;
        }

        // It shouldn't take long to receive the rest of the payload after we get the header.
        size_t remainingBytes = msg.payload_size() + CHECKSUM_SIZE;
        auto startTime = std::chrono::steady_clock::now();
        while (port.available() < remainingBytes)
        {
            delay(1);

            if (std::chrono::steady_clock::now() - startTime > std::chrono::seconds(30))
            {
                log_web(F("Serial port message could not be received within 30s (got %u / %u bytes)"), port.available(), remainingBytes);
                resync_rx();
                return false;
            }
        }

        if (port.readBytes(msg.payload(), remainingBytes) < remainingBytes)
        {
            log_web(F("Serial port payload read failure!"));
            resync_rx();
            return false;
        }

        msg.increment_write_offset(msg.payload_size()); // Don't count checksum byte.

        if (!msg.verify_checksum())
        {
            resync_rx();
            return false;
        }

        auto& config = config_instance();
        if (config.DumpPackets)
        {
            msg.debug_dump_packet();
        }

        ++rxMsgCount;
        return true;
    }

    void begin_connect()
    {
        Message cmd{MsgType::CONNECT_CMD};
        char payload[2] = {0xCA, 0x01};
        cmd.write_payload(payload, sizeof(payload));

        {
            std::lock_guard<std::mutex> lock{cmdQueueMutex};
            cmdQueue.emplace_back(std::move(cmd));
        }

        cmdQueueCv.notify_one();
    }

    void begin_get_status()
    {
        {
            std::unique_lock<std::mutex> lock{cmdQueueMutex};

            // Avoid building up an unbounded number of status updates in the command queue
            // if for some reason we're not processing them. To avoid accidentally interrupting
            // MQTT set requests and such from home assistant, only clear other GET commands.
            if (!cmdQueue.empty())
            {
                auto end = std::end(cmdQueue);
                auto it = std::remove_if(std::begin(cmdQueue), end, [](Message const& msg)
                {
                    return msg.type() == MsgType::GET_CMD;
                });

                if (it != end)
                {
                    log_web(F("command queue was not empty when queueing status query: %u"), cmdQueue.size());
                    cmdQueue.erase(it, end);
                }
            }

            cmdQueue.emplace_back(MsgType::GET_CMD, GetType::DEFROST_STATE);
            cmdQueue.emplace_back(MsgType::GET_CMD, GetType::COMPRESSOR_FREQUENCY);
            cmdQueue.emplace_back(MsgType::GET_CMD, GetType::FORCED_DHW_STATE);
            cmdQueue.emplace_back(MsgType::GET_CMD, GetType::HEATING_POWER);
            cmdQueue.emplace_back(MsgType::GET_CMD, GetType::TEMPERATURE_CONFIG);
            cmdQueue.emplace_back(MsgType::GET_CMD, GetType::SH_TEMPERATURE_STATE);
            cmdQueue.emplace_back(MsgType::GET_CMD, GetType::DHW_TEMPERATURE_STATE_A);
            cmdQueue.emplace_back(MsgType::GET_CMD, GetType::DHW_TEMPERATURE_STATE_B);
            // cmdQueue.emplace(MsgType::GET_CMD, GetType::ACTIVE_TIME);
            cmdQueue.emplace_back(MsgType::GET_CMD, GetType::FLOW_RATE);
            cmdQueue.emplace_back(MsgType::GET_CMD, GetType::MODE_FLAGS_A);
            cmdQueue.emplace_back(MsgType::GET_CMD, GetType::MODE_FLAGS_B);
            cmdQueue.emplace_back(MsgType::GET_CMD, GetType::ENERGY_USAGE);
            cmdQueue.emplace_back(MsgType::GET_CMD, GetType::ENERGY_DELIVERY);
        }

        cmdQueueCv.notify_one();
    }

    String get_device_model()
    {
        return F("Ecodan Air Source Heat Pump");
    }

    Status& get_status()
    {
        return status;
    }

    float get_temperature_step()
    {
        return temperatureStep;
    }

    float get_min_thermostat_temperature()
    {
        return 8.0f;
    }

    float get_max_thermostat_temperature()
    {
        return 28.0f;
    }

    void set_z1_target_temperature(float newTemp)
    {
        if (newTemp > get_max_thermostat_temperature())
        {
            log_web(F("Thermostat setting exceeds maximum allowed!"));
            return;
        }

        if (newTemp < get_min_thermostat_temperature())
        {
            log_web(F("Thermostat setting is lower than minimum allowed!"));
            return;
        }

        Message cmd{MsgType::SET_CMD, SetType::BASIC_SETTINGS};
        cmd[1] = SET_SETTINGS_FLAG_ZONE_TEMPERATURE;
        cmd[2] = static_cast<uint8_t>(SetZone::ZONE_1);
        cmd.set_float16(newTemp, 10);

        {
            std::lock_guard<std::mutex> lock{cmdQueueMutex};
            cmdQueue.emplace_back(std::move(cmd));
        }

        cmdQueueCv.notify_one();
    }

    // From FTC6 installation manual ("DHW max. temp.")
    float get_min_dhw_temperature()
    {
        return 40.0f;
    }

    float get_max_dhw_temperature()
    {
        return 60.0f;
    }

    // From FTC6 installation manual ("Zone heating/cooling min. temp.")
    float get_min_flow_target_temperature(String mode)
    {
        String coolMode = "Cool Flow Temperature";
        return (coolMode == mode) ? 5.0f : 20.0f;
    }

    float get_max_flow_target_temperature(String mode)
    {
        String coolMode = "Cool Flow Temperature";
        return (coolMode == mode) ? 25.0f : 60.0f;
    }

    void set_z1_flow_target_temperature(float newTemp)
    {
        if (newTemp > get_max_flow_target_temperature(status.hp_mode_as_string()))
        {
            log_web(F("Z1 flow temperature setting exceeds maximum allowed (%s)!"), String(get_max_flow_target_temperature(status.hp_mode_as_string())).c_str());
            return;
        }

        if (newTemp < get_min_flow_target_temperature(status.hp_mode_as_string()))
        {
            log_web(F("Z1 flow temperature setting is lower than minimum allowed (%s)!"), String(get_min_flow_target_temperature(status.hp_mode_as_string())).c_str());
            return;
        }

        Message cmd{MsgType::SET_CMD, SetType::BASIC_SETTINGS};
        cmd[1] = SET_SETTINGS_FLAG_ZONE_TEMPERATURE;
        cmd[2] = static_cast<uint8_t>(SetZone::ZONE_1);
        cmd[6] = static_cast<uint8_t>(SetHpMode::FLOW_CONTROL_MODE);
        cmd.set_float16(newTemp, 10);

        {
            std::lock_guard<std::mutex> lock{cmdQueueMutex};
            cmdQueue.emplace_back(std::move(cmd));
        }

        cmdQueueCv.notify_one();
    }

    void set_dhw_target_temperature(float newTemp)
    {
        if (newTemp > get_max_dhw_temperature())
        {
            log_web(F("DHW setting exceeds maximum allowed (%s)!"), String(get_max_dhw_temperature()).c_str());
            return;
        }

        if (newTemp < get_min_dhw_temperature())
        {
            log_web(F("DHW setting is lower than minimum allowed (%s)!"), String(get_min_dhw_temperature()).c_str());
            return;
        }

        Message cmd{MsgType::SET_CMD, SetType::BASIC_SETTINGS};
        cmd[1] = SET_SETTINGS_FLAG_DHW_TEMPERATURE;
        cmd.set_float16(newTemp, 8);

        {
            std::lock_guard<std::mutex> lock{cmdQueueMutex};
            cmdQueue.emplace_back(std::move(cmd));
        }

        cmdQueueCv.notify_one();
    }

    void set_dhw_mode(String mode)
    {
        Status::DhwMode dhwMode = Status::DhwMode::NORMAL;

        if (mode == "off")
        {
            set_dhw_force(false);
            return;
        }
        else if (mode == "performance")
        {
            dhwMode = Status::DhwMode::NORMAL;
        }
        else if (mode == "eco")
        {
            dhwMode = Status::DhwMode::ECO;
        }
        else
        {
            log_web(F("Unknown DHW mode set request: %s"), mode.c_str());
            return;
        }

        Message cmd{MsgType::SET_CMD, SetType::BASIC_SETTINGS};
        cmd[1] = SET_SETTINGS_FLAG_DHW_MODE;
        cmd[5] = static_cast<uint8_t>(dhwMode);

        {
            std::lock_guard<std::mutex> lock{cmdQueueMutex};
            cmdQueue.emplace_back(std::move(cmd));
        }

        cmdQueueCv.notify_one();
    }

    void set_dhw_force(bool on)
    {
        Message cmd{MsgType::SET_CMD, SetType::DHW_SETTING};
        cmd[1] = SET_SETTINGS_FLAG_MODE_TOGGLE;
        cmd[3] = on ? 1 : 0; // bit[3] of payload is DHW force, bit[2] is Holiday mode.

        {
            std::lock_guard<std::mutex> lock{cmdQueueMutex};
            cmdQueue.emplace_back(std::move(cmd));
        }

        cmdQueueCv.notify_one();
    }

    void set_power_mode(bool on)
    {
        Message cmd{MsgType::SET_CMD, SetType::BASIC_SETTINGS};
        cmd[1] = SET_SETTINGS_FLAG_MODE_TOGGLE;
        cmd[3] = on ? 1 : 0;

        {
            std::lock_guard<std::mutex> lock{cmdQueueMutex};
            cmdQueue.emplace_back(std::move(cmd));
        }

        cmdQueueCv.notify_one();
    }

    void set_hp_mode(uint8_t mode)
    {
        Message cmd{MsgType::SET_CMD, SetType::BASIC_SETTINGS};
        cmd[1] = SET_SETTINGS_FLAG_HP_MODE;
        cmd[6] = mode;

        {
            std::lock_guard<std::mutex> lock{cmdQueueMutex};
            cmdQueue.emplace_back(std::move(cmd));
        }

        cmdQueueCv.notify_one();
    }

    void handle_set_response(Message& res)
    {
        if (res.type() != MsgType::SET_RES)
        {
            log_web(F("Unexpected set response type: %#x"), static_cast<uint8_t>(res.type()));
        }
    }

    void handle_get_response(Message& res)
    {
        {
            std::lock_guard<Status> lock{status};

            switch (res.payload_type<GetType>())
            {
            case GetType::DEFROST_STATE:
                status.DefrostActive = res[3] != 0;
                break;
            case GetType::COMPRESSOR_FREQUENCY:
                status.CompressorFrequency = res[1];
                break;
            case GetType::FORCED_DHW_STATE:
                status.DhwForcedActive = res[7] != 0;
                break;
            case GetType::HEATING_POWER:
                status.OutputPower = res[6];
                break;
            case GetType::TEMPERATURE_CONFIG:
                status.Zone1SetTemperature = res.get_float16(1);
                status.Zone2SetTemperature = res.get_float16(3);
                status.Zone1FlowTemperatureSetPoint = res.get_float16(5);
                status.Zone2FlowTemperatureSetPoint = res.get_float16(7);
                status.LegionellaPreventionSetPoint = res.get_float16(9);
                status.DhwTemperatureDrop = res.get_float8_v2(11);
                status.MaximumFlowTemperature = res.get_float8_v3(12);
                status.MinimumFlowTemperature = res.get_float8_v3(13);
                break;
            case GetType::SH_TEMPERATURE_STATE:
                status.Zone1RoomTemperature = res.get_float16(1);
                if (res.get_u16(3) != 0xF0C4) // 0xF0C4 seems to be a sentinel value for "not reported in the current system"
                    status.Zone2RoomTemperature = res.get_float16(3);
                else
                    status.Zone2RoomTemperature = 0.0f;
                status.OutsideTemperature = res.get_float8(11);
                break;
            case GetType::DHW_TEMPERATURE_STATE_A:
                status.DhwFeedTemperature = res.get_float16(1);
                status.DhwReturnTemperature = res.get_float16(4);
                status.DhwTemperature = res.get_float16(7);
                break;
            case GetType::DHW_TEMPERATURE_STATE_B:
                status.BoilerFlowTemperature = res.get_float16(1);
                status.BoilerReturnTemperature = res.get_float16(4);
                break;
            case GetType::ACTIVE_TIME:
                break;
            case GetType::FLOW_RATE:
                status.FlowRate = res[12];
                break;
            case GetType::MODE_FLAGS_A:
                status.set_power_mode(res[3]);
                status.set_operation_mode(res[4]);
                status.set_dhw_mode(res[5]);
                status.set_heating_cooling_mode(res[6]);
                status.DhwFlowTemperatureSetPoint = res.get_float16(8);
                status.RadiatorFlowTemperatureSetPoint = res.get_float16(12);
                break;
            case GetType::MODE_FLAGS_B:
                status.HolidayMode = res[4] > 0;
                status.DhwTimerMode = res[5] > 0;
                break;
            case GetType::ENERGY_USAGE:
                status.EnergyConsumedHeating = res.get_float24(4);
                status.EnergyConsumedCooling = res.get_float24(7);
                status.EnergyConsumedDhw = res.get_float24(10);
                break;
            case GetType::ENERGY_DELIVERY:
                status.EnergyDeliveredHeating = res.get_float24(4);
                status.EnergyDeliveredCooling = res.get_float24(7);
                status.EnergyDeliveredDhw = res.get_float24(10);
                break;
            default:
                log_web(F("Unknown response type received on serial port: %u"), static_cast<uint8_t>(res.payload_type<GetType>()));
                break;
            }
        }
    }

    void handle_connect_response(Message& res)
    {
        log_web(F("connection reply received from heat pump"));

        std::lock_guard<std::mutex> lock{cmdQueueMutex};
        connected = true;
    }

    void handle_ext_connect_response(Message& res)
    {
        log_web(F("Unexpected extended connection response!"));
    }

    void IRAM_ATTR serial_rx_isr()
    {
        BaseType_t higherPriorityTaskWoken = pdFALSE;
        vTaskNotifyGiveIndexedFromISR(serialRxTaskHandle, 0, &higherPriorityTaskWoken);
#if CONFIG_IDF_TARGET_ESP32C3
        portEND_SWITCHING_ISR(higherPriorityTaskWoken);
#else
        portYIELD_FROM_ISR(higherPriorityTaskWoken);
#endif
    }

    void serial_rx_thread()
    {
        ehal::add_thread_to_watchdog();

        // Wake the serial RX thread when the serial RX GPIO pin changes (this may occur during or after packet receipt)
        serialRxTaskHandle = xTaskGetCurrentTaskHandle();

        {
            auto& config = config_instance();
            attachInterrupt(digitalPinToInterrupt(config.SerialRxPort), serial_rx_isr, FALLING);
        }

        while (true)
        {
            try
            {
                ehal::ping_watchdog();

                Message res;
                if (!serial_rx(res))
                {
                    continue;
                }

                switch (res.type())
                {
                case MsgType::SET_RES:
                    handle_set_response(res);
                    break;
                case MsgType::GET_RES:
                    handle_get_response(res);
                    break;
                case MsgType::CONNECT_RES:
                    handle_connect_response(res);
                    break;
                case MsgType::EXT_CONNECT_RES:
                    handle_ext_connect_response(res);
                    break;
                default:
                    log_web(F("Unknown serial message type received: %#x"), static_cast<uint8_t>(res.type()));
                    break;
                }
            }
            catch (std::exception const& ex)
            {
                ehal::log_web(F("Exception occurred on serial rx thread: %s"), ex.what());
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
    }

    void serial_tx_thread()
    {
        ehal::add_thread_to_watchdog();

        Message msg;

        while (true)
        {
            try
            {
                ehal::ping_watchdog();

                {
                    std::unique_lock<std::mutex> lock{cmdQueueMutex};

                    bool hasMessage = cmdQueueCv.wait_for(lock, std::chrono::seconds(5), [&]()
                    {
                        return !cmdQueue.empty();
                    });

                    // We timed out, so ping the watchdog and go back to waiting.
                    if (!hasMessage)
                        continue;

                    msg = std::move(cmdQueue.front());
                    cmdQueue.pop_front();
                }

                // 2400 baud is actually pretty slow
                for (int i = 0; (port.availableForWrite() < msg.size()) && i < 250; ++i)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }

                if (port.availableForWrite() < msg.size())
                {
                    log_web(F("Abandoning message write, serial port did not become writable in time: %u < %u"), port.availableForWrite(), msg.size());
                    continue;
                }

                bool success = serial_tx(msg);

                if (!success)
                {
                    std::lock_guard<std::mutex> lock{cmdQueueMutex};
                    connected = false;
                }
            }
            catch (std::exception const& ex)
            {
                ehal::log_web(F("Exception occurred on serial tx thread: %s"), ex.what());
            }
        }
    }

    bool initialize()
    {
        auto& config = config_instance();

        log_web(F("Initializing HeatPump with serial rx: %d, tx: %d"), (int8_t)config.SerialRxPort, (int8_t)config.SerialTxPort);

        pinMode(config.SerialRxPort, INPUT_PULLUP);
        pinMode(config.SerialTxPort, OUTPUT);

        delay(25); // There seems to be a window after setting the pin modes where trying to use the UART can be flaky, so introduce a short delay

        port.begin(2400, SERIAL_8E1, config.SerialRxPort, config.SerialTxPort);

        serialRxThread = std::thread{serial_rx_thread};
        serialTxThread = std::thread{serial_tx_thread};

        begin_connect();

        return true;
    }

    void handle_loop()
    {
        if (!is_connected() && !port.available())
        {
            static auto last_attempt = std::chrono::steady_clock::now();
            auto now = std::chrono::steady_clock::now();
            if (now - last_attempt > std::chrono::seconds(10))
            {
                last_attempt = now;
                begin_connect();
            }

            return;
        }
        else if (is_connected())
        {
            // Update status the first time we enter this condition, then every 60s thereafter.
            auto now = std::chrono::steady_clock::now();
            static auto last_update = now - std::chrono::seconds(120);
            if (now - last_update > std::chrono::seconds(60))
            {
                last_update = now;
                begin_get_status();
            }
        }
    }

    bool is_connected()
    {
        std::lock_guard<std::mutex> lock{cmdQueueMutex};
        return connected;
    }

    uint64_t get_tx_msg_count()
    {
        return txMsgCount;
    }

    uint64_t get_rx_msg_count()
    {
        return rxMsgCount;
    }
} // namespace ehal::hp