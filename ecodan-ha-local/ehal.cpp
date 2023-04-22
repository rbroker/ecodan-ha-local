#include "ehal.h"

namespace ehal
{
    String device_mac()
    {
        char deviceMac[17] = {};
        snprintf(deviceMac, sizeof(deviceMac), "%llx", ESP.getEfuseMac());
        return deviceMac;
    }
} // namespace ehal