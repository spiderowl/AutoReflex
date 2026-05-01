// AutoReflex - KeySender (Phase 3+)
// Simulates key presses

#pragma once

struct PluginContext;

namespace AutoReflex {
namespace Game {

// Execute a key press simulation
void ExecuteKeyPress(int virtualKeyCode, float holdDuration, PluginContext* ctx);

} // namespace Game
} // namespace AutoReflex