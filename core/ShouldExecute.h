// AutoReflex - ShouldExecute
// Gate check: are we in a game state where rules are allowed to fire?
//
// Takes the already-fetched snapshot to avoid extra GetSnapshot() calls
// on the hot path. Writes a short reason string into outReason.

#pragma once

#include <string>

struct PluginContext;
namespace PluginSDK { struct PluginGameSnapshot; }

namespace AutoReflex {

bool ShouldExecute(PluginContext* ctx,
                   const PluginSDK::PluginGameSnapshot* snapshot,
                   std::string& outReason);

} // namespace AutoReflex
