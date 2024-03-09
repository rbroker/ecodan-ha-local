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
        bool DhwForcedActive;
        uint8_t OutputPower;

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
            DHW_ON = 1,
            SH_ON = 2, // Heating
            COOL_ON = 3, // Cooling
            FROST_PROTECT = 5,
            LEGIONELLA_PREVENTION = 6
        };

        enum class DhwMode : uint8_t
        {
            NORMAL = 0,
            ECO = 1
        };

        enum class HpMode : uint8_t
        {
            HEAT_ROOM_TEMP = 0,
            HEAT_FLOW_TEMP = 1,
            HEAT_COMPENSATION_CURVE = 2,
            COOL_ROOM_TEMP = 3,
            COOL_FLOW_TEMP = 4
        };

        // Modes
        PowerMode Power;
        OperationMode Operation;
        bool HolidayMode;
        bool DhwTimerMode;
        DhwMode HotWaterMode;
        HpMode HeatingCoolingMode;

        // Efficiency
        uint8_t CompressorFrequency;
        float EnergyConsumedHeating;
        float EnergyConsumedCooling;
        float EnergyDeliveredHeating;
        float EnergyDeliveredCooling;
        float EnergyConsumedDhw;
        float EnergyDeliveredDhw;

        // HomeAssistant is a bit restrictive about what is let's us specify
        // as the mode/action of a climate integration.
        String ha_mode_as_string()
        {
            switch (Power)
            {
                case PowerMode::ON:
                    switch (HeatingCoolingMode)
                    {
                      case HpMode::HEAT_ROOM_TEMP:
                          [[fallthrough]]
                      case HpMode::HEAT_FLOW_TEMP:
                          [[fallthrough]]
                      case HpMode::HEAT_COMPENSATION_CURVE:
                          return F("heat");
                      case HpMode::COOL_ROOM_TEMP:
                          [[fallthrough]]
                      case HpMode::COOL_FLOW_TEMP:
                          return F("cool");                                                                   
                    }                    
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
                case OperationMode::FROST_PROTECT:                    
                    return F("heating");
                case OperationMode::COOL_ON:
                    return F("cooling");                        
                case OperationMode::OFF:
                    [[fallthrough]]
                case OperationMode::DHW_ON:
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
                case OperationMode::COOL_ON:                  
                    return F("Space Cooling");                        
                case OperationMode::FROST_PROTECT:
                    return F("Frost Protection");
                case OperationMode::LEGIONELLA_PREVENTION:
                    return F("Legionella Prevention");
                default:
                    return F("Unknown");
            }
        }

        String dhw_mode_as_string()
        {
            switch (Operation)
            {
                case OperationMode::DHW_ON:
                case OperationMode::LEGIONELLA_PREVENTION:
                    break;
                default:
                    return F("Off");
            }

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

        String hp_mode_as_string()
        {
            switch (HeatingCoolingMode)
            {
                case HpMode::HEAT_ROOM_TEMP:
                    return F("Heat Target Temperature");
                case HpMode::HEAT_FLOW_TEMP:
                    return F("Heat Flow Temperature");
                case HpMode::HEAT_COMPENSATION_CURVE:
                    return F("Heat Compensation Curve");
                case HpMode::COOL_ROOM_TEMP:
                    return F("Cool Target Temperature");
                case HpMode::COOL_FLOW_TEMP:
                    return F("Cool Flow Temperature");                    
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

        void set_heating_cooling_mode(uint8_t mode)
        {
            HeatingCoolingMode = static_cast<HpMode>(mode);
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
    float get_min_dhw_temperature();
    float get_max_dhw_temperature();
    float get_min_flow_target_temperature(String mode);
    float get_max_flow_target_temperature(String mode);

    bool set_z1_target_temperature(float value);
    bool set_z1_flow_target_temperature(float value);
    bool set_dhw_target_temperature(float value);
    bool set_dhw_mode(String mode);
    bool set_dhw_force(bool on);
    bool set_power_mode(bool on);
    bool set_hp_mode(uint8_t mode);

    bool begin_connect();
    bool begin_update_status();

    bool initialize();
    void handle_loop();
    bool is_connected();

    uint64_t get_rx_msg_count();
    uint64_t get_tx_msg_count();
} // namespace ehal::hp