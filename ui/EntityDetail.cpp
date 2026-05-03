// EntityDetail.cpp - Selected entity detail panel with components tree
// Matches ExamplePlugin/examples/ExampleEntities.h one-to-one

#include "EntityDetail.h"
#include "../sdk/PluginHelpers.h"
#include "../imgui/imgui.h"
#include <limits>

namespace AutoReflex {

// ─── Helpers (same as ExamplePlugin) ─────────────────────────────────────────

static void DrawAddressRow(const char* label, uintptr_t addr) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn(); ImGui::Text("%s", label);
    ImGui::TableNextColumn();
    char buf[20]; std::snprintf(buf, sizeof(buf), "0x%llX", (unsigned long long)addr);
    ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.6f, 1.0f), "%s", buf);
    if (ImGui::IsItemHovered() && ImGui::IsItemClicked()) ImGui::SetClipboardText(buf);
}

// ─── Component renderers (exact match to ExamplePlugin) ──────────────────────

static void DrawLifeComp(const PluginSDK::DebugLifeComp& cl, uint32_t /*entityId*/) {
    if (ImGui::BeginTable("LifeT", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 220.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        DrawAddressRow("Address", cl.Address);
        DrawAddressRow("Owner Address", cl.OwnerAddress);
        ImGui::EndTable();
    }

    auto renderVital = [](const char* vlabel, const PluginSDK::DebugVital& v) {
        if (ImGui::TreeNode(vlabel)) {
            if (ImGui::BeginTable("VitalT", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 220.0f);
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("Regeneration");
                ImGui::TableNextColumn(); ImGui::Text("%.4f", v.Regeneration);

                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("Total");
                ImGui::TableNextColumn(); ImGui::Text("%d", v.Total);

                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("ReservedFlat");
                ImGui::TableNextColumn(); ImGui::Text("%d", v.ReservedFlat);

                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("Current");
                ImGui::TableNextColumn(); ImGui::Text("%d", v.Current);

                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("Reserved(%)");
                ImGui::TableNextColumn(); ImGui::Text("%d", v.ReservedPercent);

                if (v.Total > 0) {
                    float pct = static_cast<float>(v.Current) / v.Total * 100.0f;
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("Current(%)");
                    ImGui::TableNextColumn(); ImGui::Text("%.1f%%", pct);
                }
                ImGui::EndTable();
            }
            ImGui::TreePop();
        }
    };
    renderVital("Health", cl.Health);
    renderVital("Energy Shield", cl.EnergyShield);
    renderVital("Mana", cl.Mana);
}

static void DrawRenderComp(const PluginSDK::DebugRenderComp& cr) {
    if (ImGui::BeginTable("RenderT", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 220.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        DrawAddressRow("Address", cr.Address);
        DrawAddressRow("Owner Address", cr.OwnerAddress);
        ImGui::EndTable();
    }
    ImGui::Text("Grid Position: {%.2f, %.2f}", cr.GridX, cr.GridY);
    ImGui::Text("World Position: {%.2f, %.2f, %.2f}", cr.WorldX, cr.WorldY, cr.WorldZ);
    if (ImGui::BeginTable("RenderT2", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 220.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("Terrain Height (Z-Axis)");
        ImGui::TableNextColumn(); ImGui::Text("%.4f", cr.TerrainHeight);
        ImGui::EndTable();
    }
    ImGui::Text("Model Bounds: {%.2f, %.2f, %.2f}", cr.ModelBoundsX, cr.ModelBoundsY, cr.ModelBoundsZ);
}

static void DrawPositionedComp(const PluginSDK::DebugPositionedComp& cp) {
    if (ImGui::BeginTable("PosT", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 220.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        DrawAddressRow("Address", cp.Address);
        DrawAddressRow("Owner Address", cp.OwnerAddress);
        ImGui::EndTable();
    }
    ImGui::Text("Flags: 0x%02X", cp.Reaction);
    if (ImGui::BeginTable("PosT2", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 220.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("IsFriendly");
        ImGui::TableNextColumn(); ImGui::TextColored(cp.IsFriendly ? ImVec4(0.3f,1,0.3f,1) : ImVec4(1,0.3f,0.3f,1), cp.IsFriendly ? "true" : "false");
        ImGui::EndTable();
    }
}

static void DrawTargetableComp(const PluginSDK::DebugTargetableComp& ct) {
    if (ImGui::BeginTable("TgtT", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 220.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        DrawAddressRow("Address", ct.Address);
        DrawAddressRow("Owner Address", ct.OwnerAddress);

        auto boolRow = [](const char* name, bool val) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("%s", name);
            ImGui::TableNextColumn(); ImGui::TextColored(val ? ImVec4(0.3f,1,0.3f,1) : ImVec4(1,0.3f,0.3f,1), val ? "true" : "false");
        };
        boolRow("IsHighlightable", ct.IsHighlightable);
        boolRow("IsTargettedByPlayer", ct.IsTargettedByPlayer);
        boolRow("IsTargetable", ct.IsTargetable);
        boolRow("HiddenFromPlayer", ct.HiddenFromPlayer);
        boolRow("MeetsQuestState", ct.MeetsQuestState);
        boolRow("MeetsItemRequirements", ct.MeetsItemRequirements);
        ImGui::EndTable();
    }
}

static void DrawAnimatedComp(const PluginSDK::DebugAnimatedComp& ca) {
    if (ImGui::BeginTable("AnimT", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 220.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        DrawAddressRow("Address", ca.Address);
        DrawAddressRow("Owner Address", ca.OwnerAddress);
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("Path");
        ImGui::TableNextColumn(); ImGui::TextWrapped("%s", ca.Path.c_str());
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("Id");
        ImGui::TableNextColumn(); ImGui::Text("%u", ca.Id);
        ImGui::EndTable();
    }
}

static void DrawStatsComp(const PluginSDK::DebugStatsComp& cs, uint32_t entityId) {
    if (ImGui::BeginTable("StatsT", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 220.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        DrawAddressRow("Address", cs.Address);
        DrawAddressRow("Owner Address", cs.OwnerAddress);
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("CurrentWeaponIndex");
        ImGui::TableNextColumn(); ImGui::Text("%d", cs.CurrentWeaponIndex);
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("IsInShapeshiftedForm");
        ImGui::TableNextColumn(); ImGui::TextColored(cs.IsShapeshifted ? ImVec4(0.3f,1,0.3f,1) : ImVec4(1,0.3f,0.3f,1), cs.IsShapeshifted ? "true" : "false");
        ImGui::EndTable();
    }
    char treeId[32];
    std::snprintf(treeId, sizeof(treeId), "Items##si%u", entityId);
    if (ImGui::TreeNode(treeId)) {
        for (const auto& [k, v] : cs.StatsItems)
            ImGui::Text("[%d]: %d", k, v);
        ImGui::TreePop();
    }
    std::snprintf(treeId, sizeof(treeId), "BuffAndActions##sb%u", entityId);
    if (ImGui::TreeNode(treeId)) {
        for (const auto& [k, v] : cs.StatsBuff)
            ImGui::Text("[%d]: %d", k, v);
        ImGui::TreePop();
    }
}

static void DrawActorComp(const PluginSDK::DebugActorComp& cac, uint32_t entityId) {
    if (ImGui::BeginTable("ActorT", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 220.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        DrawAddressRow("Address", cac.Address);
        DrawAddressRow("Owner Address", cac.OwnerAddress);
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("Animation");
        ImGui::TableNextColumn(); ImGui::Text("%s", cac.AnimationName.c_str());
        ImGui::EndTable();
    }
    char treeId[32];
    std::snprintf(treeId, sizeof(treeId), "Skills##as%u", entityId);
    if (!cac.ActiveSkills.empty() && ImGui::TreeNode(treeId)) {
        for (const auto& skill : cac.ActiveSkills) {
            if (ImGui::TreeNode(skill.Name.c_str())) {
                if (ImGui::BeginTable("SkillT", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
                    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 220.0f);
                    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("Use Stage");
                    ImGui::TableNextColumn(); ImGui::Text("%d", skill.UseStage);
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("Cast Type");
                    ImGui::TableNextColumn(); ImGui::Text("%d", skill.CastType);
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("Total Uses");
                    ImGui::TableNextColumn(); ImGui::Text("%d", skill.TotalUses);
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("Cooldown Time (ms)");
                    ImGui::TableNextColumn(); ImGui::Text("%d", skill.TotalCooldownTimeInMs);
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("Can Be Used");
                    ImGui::TableNextColumn(); ImGui::TextColored(skill.CanBeUsed ? ImVec4(0.3f,1,0.3f,1) : ImVec4(1,0.3f,0.3f,1), skill.CanBeUsed ? "true" : "false");
                    ImGui::EndTable();
                }
                ImGui::TreePop();
            }
        }
        ImGui::TreePop();
    }
    std::snprintf(treeId, sizeof(treeId), "Deployed##dp%u", entityId);
    if (ImGui::TreeNode(treeId)) {
        for (int i = 0; i < 256; i++) {
            if (cac.DeployedCounts[i] > 0)
                ImGui::Text("Object Type: %d, Total Count: %d", i, cac.DeployedCounts[i]);
        }
        ImGui::TreePop();
    }
}

static void DrawBuffsComp(const std::vector<PluginSDK::DebugBuff>& buffs, uint32_t entityId) {
    ImGui::Text("Effects: %zu", buffs.size());
    char treeId[32];
    std::snprintf(treeId, sizeof(treeId), "Effects##bf%u", entityId);
    if (!buffs.empty() && ImGui::TreeNode(treeId)) {
        for (const auto& buff : buffs) {
            if (ImGui::TreeNode(buff.Name.c_str())) {
                if (ImGui::BeginTable("BuffT", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
                    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 220.0f);
                    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("Name");
                    ImGui::TableNextColumn(); ImGui::Text("%s", buff.Name.c_str());
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("Total Time");
                    ImGui::TableNextColumn(); ImGui::Text("%.2f", buff.TotalTime > 0 ? buff.TotalTime : std::numeric_limits<float>::infinity());
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("Time Left");
                    ImGui::TableNextColumn(); ImGui::Text("%.2f", buff.TimeLeft > 0 ? buff.TimeLeft : std::numeric_limits<float>::infinity());
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("Source Entity Id");
                    ImGui::TableNextColumn(); ImGui::Text("%u", buff.SourceEntityId);
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("Charges");
                    ImGui::TableNextColumn(); ImGui::Text("%d", buff.Charges);
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("Source FlaskSlot");
                    ImGui::TableNextColumn(); ImGui::Text("%d", buff.FlaskSlot);
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("Source Effectiveness");
                    ImGui::TableNextColumn(); ImGui::Text("%d", 100 + buff.Effectiveness);
                    ImGui::EndTable();
                }
                ImGui::TreePop();
            }
        }
        ImGui::TreePop();
    }
}

// ─── Main entry point ───────────────────────────────────────────────────────

void DrawEntityDetail(
    PluginContext* context,
    const PluginSDK::DebugEntityInfo& entity,
    uint32_t& watchedEntityId,
    int& /*watchFrameCounter*/)
{
    // Entity info
    ImGui::Text("Name: %s", entity.Path.c_str());
    ImGui::Text("Id: %u  Address: 0x%llX", entity.Id, (unsigned long long)entity.Address);

    // Use SDK helper functions instead of manual string mapping
    const char* zoneStr = PluginSDK::GetNearbyZoneName(entity.Zone);
    const char* rarityStr = PluginSDK::GetRarityName(entity.Rarity);

    // Use SDK EntityStates enum instead of hardcoded value
    bool isFriendlyState = (static_cast<PluginSDK::EntityStates>(entity.EntityState) == PluginSDK::EntityStates::MonsterFriendly);
    ImVec4 friendlyColor = isFriendlyState ? ImVec4(0.3f, 1.0f, 0.3f, 1.0f) : ImVec4(1.0f, 0.4f, 0.4f, 1.0f);

    ImGui::Text("Zone: %s  Rarity: %s", zoneStr, rarityStr);
    ImGui::TextColored(friendlyColor, "State: %s", isFriendlyState ? "FRIENDLY" : "Hostile");

    bool likelyMinion = entity.Path.find("Minion") != std::string::npos
        || entity.Path.find("Totem") != std::string::npos
        || entity.Path.find("Sentinel") != std::string::npos
        || entity.Path.find("Summon") != std::string::npos;

    if (likelyMinion) {
        ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "  [Likely Minion based on path]");
    }
    ImGui::Text("Type: %d  SubType: %d  State: %d", entity.EntityType, entity.EntitySubType, entity.EntityState);

    // ─── Components tree (exact match to ExamplePlugin pattern) ──────────────
    char compLabel[128];
    std::snprintf(compLabel, sizeof(compLabel), "Components (%zu)##comp%u",
        entity.ComponentAddresses.size(), entity.Id);
    bool compOpen = ImGui::TreeNode(compLabel);
    if (compOpen) {
        // Start watching when tree is opened (like ExamplePlugin)
        if (context->WatchEntity) {
            context->WatchEntity(entity.Id);
        }
        watchedEntityId = entity.Id;

        auto ec = context->GetWatchedEntityData
            ? context->GetWatchedEntityData(entity.Id)
            : PluginSDK::DebugEntityComponents{};
        bool hasData = ec.Valid;

        if (!hasData) {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.3f, 1.0f), "Loading...");
        }

        for (const auto& [compName, compAddr] : entity.ComponentAddresses) {
            bool rendered = false;
            if (hasData) {
                if (compName == "Life" && ec.HasLife) {
                    if (ImGui::TreeNode("Life")) { DrawLifeComp(ec.Life, entity.Id); ImGui::TreePop(); }
                    rendered = true;
                }
                else if (compName == "Render" && ec.HasRender) {
                    if (ImGui::TreeNode("Render")) { DrawRenderComp(ec.Render); ImGui::TreePop(); }
                    rendered = true;
                }
                else if (compName == "Positioned" && ec.HasPositioned) {
                    if (ImGui::TreeNode("Positioned")) { DrawPositionedComp(ec.Positioned); ImGui::TreePop(); }
                    rendered = true;
                }
                else if (compName == "Targetable" && ec.HasTargetable) {
                    if (ImGui::TreeNode("Targetable")) { DrawTargetableComp(ec.Targetable); ImGui::TreePop(); }
                    rendered = true;
                }
                else if (compName == "Animated" && ec.HasAnimated) {
                    if (ImGui::TreeNode("Animated")) { DrawAnimatedComp(ec.Animated); ImGui::TreePop(); }
                    rendered = true;
                }
                else if (compName == "Stats" && ec.HasStats) {
                    if (ImGui::TreeNode("Stats")) { DrawStatsComp(ec.Stats, entity.Id); ImGui::TreePop(); }
                    rendered = true;
                }
                else if (compName == "Actor" && ec.HasActor) {
                    if (ImGui::TreeNode("Actor")) { DrawActorComp(ec.Actor, entity.Id); ImGui::TreePop(); }
                    rendered = true;
                }
                else if (compName == "Buffs" && ec.HasBuffs) {
                    if (ImGui::TreeNode("Buffs")) { DrawBuffsComp(ec.Buffs, entity.Id); ImGui::TreePop(); }
                    rendered = true;
                }
            }

            if (!rendered) {
                char unknownLabel[128];
                std::snprintf(unknownLabel, sizeof(unknownLabel), "%s: 0x%llX", compName.c_str(), (unsigned long long)compAddr);
                ImGui::TextDisabled("%s", unknownLabel);
            }
        }

        if (entity.ComponentAddresses.empty()) {
            ImGui::TextDisabled("(no component addresses)");
        }

        ImGui::TreePop();
    } else {
        // Unwatch when tree is closed (like ExamplePlugin)
        if (watchedEntityId == entity.Id && context->UnwatchEntity) {
            context->UnwatchEntity(entity.Id);
        }
    }
}

} // namespace AutoReflex