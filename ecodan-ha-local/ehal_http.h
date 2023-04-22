#pragma once

namespace ehal::http
{
    bool initialize_default();
    bool initialize_captive_portal();
    void handle_loop();
} // namespace ehal::http