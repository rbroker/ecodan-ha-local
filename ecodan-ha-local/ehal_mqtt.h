#pragma once

namespace ehal::mqtt
{
    String entity_name();

    bool initialize();
    void handle_loop();
    bool is_connected();
} // namespace ehal::mqtt