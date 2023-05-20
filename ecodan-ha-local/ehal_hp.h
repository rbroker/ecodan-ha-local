#pragma once

#include "Arduino.h"
#include <functional>
#include <mutex>

namespace ehal::hp
{
    struct Status
    {
        bool Initialized = false;

        bool DefrostActive;
        bool DhwBoostActive;

        float Zone1SetTemperature;
        float Zone1FlowTemperatureSetPoint;
        float Zone1RoomTemperature;
        float Zone2SetTemperature;
        float Zone2FlowTemperatureSetPoint;
        float Zone2RoomTemperature;
        float LegionellaPreventionSetPoint;
        float DhwTemperatureDrop;
        uint8_t MaximumFlowTemperature;
        uint8_t MinimumFlowTemperature;
        float OutsideTemperature;
        float DhwFeedTemperature;
        float DhwReturnTemperature;
        float DhwTemperature;
        float BoilerFlowTemperature;
        float BoilerReturnTemperature;
        uint8_t FlowRate;
        float DhwFlowTemperatureSetPoint;
        float RadiatorFlowTemperatureSetPoint;

        enum class PowerMode : uint8_t
        {
            STANDBY = 0,
            ON = 1
        };

        enum class OperationMode : uint8_t
        {
            OFF = 0,
            SH_ON = 1,
            DHW_ON = 2,
            FROST_PROTECT = 5
        };

        enum class DhwMode : uint8_t
        {
            NORMAL = 0,
            ECO = 1
        };

        enum class ShMode : uint8_t
        {
            TEMPERATURE = 0,
            FLOW_CONTROL = 1,
            COMPENSATION_CURVE = 2
        };

        // Modes
        PowerMode Power;
        OperationMode Operation;
        bool HolidayMode;
        bool DhwTimerMode;
        DhwMode HotWaterMode;
        ShMode HeatingMode;

        // Efficiency
        uint8_t CompressorFrequency;
        float EnergyConsumedHeating;
        float EnergyDeliveredHeating;
        float EnergyConsumedDhw;
        float EnergyDeliveredDhw;

        // HomeAssistant is a bit restrictive about what is let's us specify
        // as the mode/action of a climate integration.
        String ha_mode_as_string()
        {
            switch (Power)
            {
                case PowerMode::ON:
                    return F("heat");
                default:
                    return F("off");
            }
        }

        String ha_action_as_string()
        {
            switch (Operation)
            {
                case OperationMode::SH_ON:
                    [[fallthrough]]
                case OperationMode::DHW_ON:
                    [[fallthrough]]
                case OperationMode::FROST_PROTECT:
                    return F("heating");

                case OperationMode::OFF:
                    [[fallthrough]]
                default:
                    return F("idle");
            }
        }

        String power_as_string()
        {
            switch (Power)
            {
                case PowerMode::ON:
                    return F("On");
                case PowerMode::STANDBY:
                    return F("Standby");
                default:
                    return F("Unknown");
            }
        }

        String operation_as_string()
        {
            switch (Operation)
            {
                case OperationMode::OFF:
                    return F("Off");
                case OperationMode::DHW_ON:
                    return F("Heating Water");
                case OperationMode::SH_ON:
                    return F("Space Heating");
                case OperationMode::FROST_PROTECT:
                    return F("Frost Protection");
                default:
                    return F("Unknown");
            }
        }

        String dhw_mode_as_string()
        {
            switch (HotWaterMode)
            {
                case DhwMode::NORMAL:
                    return F("Normal");
                case DhwMode::ECO:
                    return F("Eco");
                default:
                    return F("Unknown");
            }
        }

        String heating_mode_as_string()
        {
            switch (HeatingMode)
            {
                case ShMode::TEMPERATURE:
                    return F("Target Temperature");
                case ShMode::FLOW_CONTROL:
                    return F("Flow Control");
                case ShMode::COMPENSATION_CURVE:
                    return F("Compensation Curve");
                default:
                    return F("Unknown");
            }
        }

        void set_power_mode(uint8_t mode)
        {
            Power = static_cast<PowerMode>(mode);
        }

        void set_operation_mode(uint8_t mode)
        {
            Operation = static_cast<OperationMode>(mode);
        }

        void set_dhw_mode(uint8_t mode)
        {
            HotWaterMode = static_cast<DhwMode>(mode);
        }

        void set_heating_mode(uint8_t mode)
        {
            HeatingMode = static_cast<ShMode>(mode);
        }

        void lock()
        {
            lock_.lock();
        }

        void unlock()
        {
            lock_.unlock();
        }

      private:
        std::mutex lock_;
    };

    String get_device_model();
    Status& get_status();

    float get_temperature_step();
    float get_min_thermostat_temperature();
    float get_max_thermostat_temperature();

    bool set_z1_target_temperature(float value);
    bool set_mode(const String& mode);

    bool begin_connect();
    bool begin_update_status();

    bool initialize();
    void handle_loop();
    bool is_connected();

    uint64_t get_rx_msg_count();
    uint64_t get_tx_msg_count();
} // namespace ehal::hp