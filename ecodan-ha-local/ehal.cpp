#include "ehal.h"

namespace ehal
{
    String device_mac()
    {
        char deviceMac[17] = {};
        snprintf_P(deviceMac, sizeof(deviceMac), (PGM_P)F("%llx"), ESP.getEfuseMac());
        return deviceMac;
    }
} // namespace ehal