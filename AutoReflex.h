// AutoReflex - POEFixer Plugin



#pragma once



#include "sdk/PluginSDK.h"



#include <string>

#include <memory>

#include <chrono>

#include <filesystem>



#include "rules/RuleManager.h"

#include "storage/RuleStore.h"

#include "storage/SettingsStore.h"

#include "core/AnimationLock.h"



namespace AutoReflex { namespace UI      { class SettingsPanel; } }

namespace AutoReflex { namespace Storage { class SettingsStore; } }



class AutoReflexPlugin : public PluginSDK::Plugin {

    friend class AutoReflex::UI::SettingsPanel;

    friend class AutoReflex::Storage::SettingsStore;



public:

    const char* GetName() const override { return "AutoReflex"; }



    void OnEnable(bool isGameAttached) override;



    void OnDisable() override;



    void DrawSettings() override;



    void DrawUI() override;



    void SaveSettings() override;



    bool WantsOverlay() const override { return true; }



    const std::string& GetLastExecutionGateReason() const { return m_LastExecutionGateReason; }



private:

    void LoadSettings();

    void ConfigureBuffsDebugDumpPaths();

    void SubscribeToHostEvents();

    void UnsubscribeFromHostEvents();



    std::unique_ptr<AutoReflex::Rules::RuleManager>     m_RuleManager;

    std::unique_ptr<AutoReflex::Storage::RuleStore>     m_RuleStore;

    std::unique_ptr<AutoReflex::Storage::SettingsStore> m_SettingsStore;

    AutoReflex::Core::AnimationLock                     m_AnimationLock;



    PluginSDK::EventsService::Token m_AreaChangeToken{};

    PluginSDK::EventsService::Token m_GameDetachedToken{};



    bool                                  m_TestFireEnabled     = false;

    bool                                  m_ShowGateReason      = false;

    float                                 m_TestFireCooldownSec = 0.8f;

    std::chrono::steady_clock::time_point m_LastTestFire;



    int m_SelectedRuleIndex = -1;



    std::string m_LastExecutionGateReason = "Idle";

};


