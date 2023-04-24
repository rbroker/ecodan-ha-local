#pragma once

namespace ehal::mqtt
{
    String unique_entity_name(const String& name);

    bool initialize();
    void handle_loop();
    bool is_connected();
} // namespace ehal::mqtt