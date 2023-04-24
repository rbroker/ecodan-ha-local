#include "ehal.h"
#include "ehal_config.h"
#include "ehal_diagnostics.h"
#include "ehal_hp.h"
#include "ehal_proto.h"

#include <HardwareSerial.h>

#include <mutex>
#include <queue>
#include <thread>

namespace ehal::hp
{
    HardwareSerial port = Serial1;

    bool debugDumpPackets = false;
    uint64_t rxMsgCount = 0;
    uint64_t txMsgCount = 0;
    std::thread serialRxThread;
    std::queue<Message> getStatusCmdQueue;
    std::mutex getStatusCmdQueueMutex;

    Status status;
    float temperatureStep = 0.5f;
    bool connected = false;
    OnConnectionEstablishedCallback onConnectionEstablished = nullptr;
    OnStatusUpdatedCallback onStatusUpdated = nullptr;

    bool serial_tx(Message& msg)
    {
        if (!port)
        {
            log_web_ratelimit("Serial connection unavailable for tx");
            return false;
        }

        if (port.availableForWrite() < msg.size())
        {
            log_web("Serial tx buffer size: %u", port.availableForWrite());
            return false;
        }

        msg.set_checksum();
        port.write(msg.buffer(), msg.size());

        if (debugDumpPackets)
        {
            msg.debug_dump_packet();
        }

        ++txMsgCount;
        return true;
    }

    bool serial_rx(Message& msg)
    {
        if (!port)
        {
            log_web_ratelimit("Serial connection unavailable for rx");
            return false;
        }

        if (port.available() < HEADER_SIZE)
        {
            return false;
        }

        // Scan for the start of an Ecodan packet.
        if (port.peek() != HEADER_MAGIC_A)
        {
            log_web_ratelimit("Dropping serial data, header magic mismatch");
            port.read();
            return false;
        }

        if (port.readBytes(msg.buffer(), HEADER_SIZE) < HEADER_SIZE)
        {
            log_web("Serial port header read failure!");
            return false;
        }

        msg.increment_write_offset(HEADER_SIZE);

        if (!msg.verify_header())
        {
            log_web("Serial port message appears invalid, skipping payload wait...");
            return false;
        }

        // It shouldn't take long to receive the rest of the payload after we get the header.
        size_t remainingBytes = msg.payload_size() + CHECKSUM_SIZE;        
        while (port.available() < remainingBytes)
        {            
            delay(1);
        }

        if (port.readBytes(msg.payload(), remainingBytes) < remainingBytes)
        {
            log_web("Serial port payload read failure!");
            return false;
        }

        msg.increment_write_offset(msg.payload_size()); // Don't count checksum byte.

        if (!msg.verify_checksum())
            return false;

        if (debugDumpPackets)
        {
            msg.debug_dump_packet();
        }

        ++rxMsgCount;
        return true;
    }

    bool begin_connect(OnConnectionEstablishedCallback callback)
    {
        Message cmd{MsgType::CONNECT_CMD};
        char payload[2] = {0xCA, 0x01};
        cmd.write_payload(payload, sizeof(payload));

        if (!serial_tx(cmd))
        {
            log_web("Failed to tx CONNECT_CMD!");
            return false;
        }

        onConnectionEstablished = callback;
        return true;
    }

    bool dispatch_next_get_status_cmd()
    {
        Message msg;
        {
            std::lock_guard<std::mutex> lock{getStatusCmdQueueMutex};

            if (getStatusCmdQueue.empty())
            {                
                return true;
            }

            msg = std::move(getStatusCmdQueue.front());
            getStatusCmdQueue.pop();
        }

        if (!serial_tx(msg))
        {
            log_web("Unable to dispatch status update request, flushing queued requests...");

            std::lock_guard<std::mutex> lock{getStatusCmdQueueMutex};
            while (!getStatusCmdQueue.empty())
                getStatusCmdQueue.pop();
            connected = false;
            return false;
        }

        return true;
    }

    bool begin_get_status(OnStatusUpdatedCallback callback)
    {
        onStatusUpdated = callback;

        {
            std::lock_guard<std::mutex> lock{getStatusCmdQueueMutex};

            if (!getStatusCmdQueue.empty())
            {
                log_web("Existing GET STATUS operation is already in progress: %u", getStatusCmdQueue.size());
                return false;
            }

            getStatusCmdQueue.emplace(MsgType::GET_CMD, GetType::DEFROST_STATE);
            getStatusCmdQueue.emplace(MsgType::GET_CMD, GetType::COMPRESSOR_FREQUENCY);
            getStatusCmdQueue.emplace(MsgType::GET_CMD, GetType::FORCED_DHW_STATE);
            // getStatusCmdQueue.emplace(MsgType::GET_CMD, GetType::HEATING_POWER ); #TODO #FIXME limited utility?
            getStatusCmdQueue.emplace(MsgType::GET_CMD, GetType::TEMPERATURE_CONFIG);
            getStatusCmdQueue.emplace(MsgType::GET_CMD, GetType::SH_TEMPERATURE_STATE);
            getStatusCmdQueue.emplace(MsgType::GET_CMD, GetType::DHW_TEMPERATURE_STATE_A);
            getStatusCmdQueue.emplace(MsgType::GET_CMD, GetType::DHW_TEMPERATURE_STATE_B);
            // getStatusCmdQueue.emplace(MsgType::GET_CMD, GetType::ACTIVE_TIME);
            getStatusCmdQueue.emplace(MsgType::GET_CMD, GetType::FLOW_RATE);
            getStatusCmdQueue.emplace(MsgType::GET_CMD, GetType::MODE_FLAGS_A);
            getStatusCmdQueue.emplace(MsgType::GET_CMD, GetType::MODE_FLAGS_B);
            getStatusCmdQueue.emplace(MsgType::GET_CMD, GetType::ENERGY_USAGE);
            getStatusCmdQueue.emplace(MsgType::GET_CMD, GetType::ENERGY_DELIVERY);
        }

        return dispatch_next_get_status_cmd();
    }

    String get_device_model()
    {
        return F("Ecodan PUZ-WM60VAA");
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

    void handle_set_response(Message& res)
    {
        log_web("Unexpected setting change response!");
    }

    void handle_get_response(Message& res)
    {
        {
            std::lock_guard<Status> lock{status};

            switch (res.payload_type())
            {
            case GetType::DEFROST_STATE:
                status.DefrostActive = res[3] != 0;
                break;
            case GetType::COMPRESSOR_FREQUENCY:
                status.CompressorFrequency = res[1];
                break;
            case GetType::FORCED_DHW_STATE:
                status.DhwBoostActive = res[7] != 0;
                break;
            case GetType::HEATING_POWER:
                break;
            case GetType::TEMPERATURE_CONFIG:
                status.Zone1SetTemperature = res.get_float16(1);
                status.Zone2SetTemperature = res.get_float16(3);
                status.Zone1FlowTemperatureSetPoint = res.get_float16(5);
                status.Zone2FlowTemperatureSetPoint = res.get_float16(7);
                status.LegionellaPreventionSetPoint = res.get_float16(9);
                status.DhwTemperatureDrop = res.get_float8(11);
                status.MaximumFlowTemperature = res.get_float8(12);
                status.MinimumFlowTemperature = res.get_float8(13);
                break;
            case GetType::SH_TEMPERATURE_STATE:
                status.Zone1RoomTemperature = res.get_float16(1);
                status.Zone2RoomTemperature = res.get_float16(7);
                status.OutsideTemperature = res.get_float16(11);
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
                status.set_heating_mode(res[6]);
                status.DhwFlowTemperatureSetPoint = res.get_float16(8);
                status.RadiatorFlowTemperatureSetPoint = res.get_float16(12);
                break;
            case GetType::MODE_FLAGS_B:
                status.HolidayMode = res[4] > 0;
                status.DhwTimerMode = res[5] > 0;
                break;
            case GetType::ENERGY_USAGE:
                status.EnergyConsumedHeating = res.get_float24(4);
                status.EnergyConsumedDhw = res.get_float24(10);
                break;
            case GetType::ENERGY_DELIVERY:
                status.EnergyDeliveredHeating = res.get_float24(4);
                status.EnergyDeliveredDhw = res.get_float24(10);
                break;
            default:
                log_web("Unknown response type received on serial port: %u", static_cast<uint8_t>(res.payload_type()));
                break;
            }
        }

        if (!dispatch_next_get_status_cmd())
        {
            log_web("Failed to dispatch status update command!");
        }
    }

    void handle_connect_response(Message& res)
    {
        log_web("connection reply received from heat pump");

        connected = true;

        if (onConnectionEstablished)
        {
            onConnectionEstablished();
        }
    }

    void handle_ext_connect_response(Message& res)
    {
        log_web("Unexpected extended connection response!");
    }

    void serial_rx_thread()
    {
        while (true)
        {
            Message res;
            if (!serial_rx(res))
            {
                delay(1);
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
                log_web("Unknown serial message type received: %#x", static_cast<uint8_t>(res.type()));
                break;
            }
        }
    }

    bool initialize()
    {
        auto& config = config_instance();

        log_web("Initializing HeatPump with serial rx: %d, tx: %d", (int8_t)config.SerialRxPort, (int8_t)config.SerialTxPort);

        // port.begin(2400, SERIAL_8E1, config.SerialRxPort, config.SerialTxPort);
        port.begin(2400, SERIAL_8N1, config.SerialRxPort, config.SerialTxPort);
        pinMode(config.SerialRxPort, INPUT_PULLUP);

        serialRxThread = std::thread{serial_rx_thread};

        if (!begin_connect(nullptr))
        {
            log_web("Failed to start heatpump connection proceedure...");
        }

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
                if (!begin_connect(nullptr))
                {
                    log_web("Failed to start heatpump connection proceedure...");
                }
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
                if (!begin_get_status(nullptr))
                {
                    log_web("Failed to begin heatpump status update!");
                }
            }
        }
    }

    bool is_connected()
    {
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