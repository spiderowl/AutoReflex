# AutoReflex — Master Build Plan

> POEFixer C++ plugin · AngelScript conditions · Monster-aware key automation

---

## What AutoReflex Does

AutoReflex is a POEFixer plugin. It monitors nearby monsters every frame and fires configurable key presses when user-written AngelScript conditions return true. Each rule is independently scripted, independently timed, and independently stored. No profiles. No presets. Just a flat list of rules you write yourself.

---

## Locked Architecture Decisions

| Decision | Choice | Reason |
|---|---|---|
| Language | C++ (POEFixer native) | Required by POEFixer plugin system |
| Scripting engine | AngelScript | C-style syntax, static typing, compile-time errors, line numbers |
| Settings format | `nlohmann/json.hpp` single header | No build setup, human-readable, handles nested data |
| Storage layout | One JSON per rule + one `settings.json` | Each rule is self-contained, no index file |
| Rule ordering | `"order"` integer field in each rule JSON | Load all files, sort by order — no filename tricks |
| Monster data | Rebuilt every frame from snapshot | No stale data, area changes handled automatically |
| Buff reads | Lazy — only when `HasBuff()` is called in script | No unnecessary memory reads |
| Key sending | `SendInput` (keydown + keyup pair) | POEFixer standard |
| Execution thread | Main render thread in `DrawUI()` | Required by POEFixer threading rules |
| Subsystem wiring | Each subsystem stores `PluginContext*` from constructor | Clean dependency injection, no globals or singletons |
| Settings ownership | All flags live in `PluginContext` | Single source of truth, no separate settings struct |

---

## Locked UI Decisions

| Decision | Choice |
|---|---|
| Key binding | Dropdown: `1 2 3 4 5`, `Q W E R`, `F1–F12` |
| Script boilerplate | Hidden — user writes body only, plugin wraps before compile |
| Compile trigger | Save button only — no live compile while typing |
| Rename behaviour | Delete old JSON, create new JSON |
| Duplicate names | Blocked — name field turns red, Save button disabled |
| Invalid filename chars | Blocked — show inline error, Save disabled |
| Broken script on Save | Saved to disk, rule force-disabled until fixed |
| Unsaved changes | Red label shown, silent discard if user clicks away |
| Empty state (no selection) | "No rule selected" text + `[+ Add Rule]` button |
| Debug Mode | Toggles debug log window |
| Default rules on first run | One example rule to show API usage |

---

## Settings Panel Layout

```
┌─────────────────────────────────────────────────────────────────────┐
│  [✓] Run in Hideout   [✓] Run in Town   [ ] Debug Mode             │
│  Status: ● Active                                                   │
├───────────────────┬─────────────────────────────────────────────────┤
│ ● RarePackCast  Q │  Name   [ RarePackCast       ]  ← red if dupe  │
│ ○ UniqueCheck   W │  Key    [ Q ▼ ]   Cooldown  ◀ 0.800s ▶         │
│ ✖ BrokenRule    E │  [✓] Enabled                                    │
│ ─ Disabled      R │  ─────────────────────────────────────────────  │
│                   │  return s.Monsters.Inner.Rare >= 3;             │
│                   │                                                 │
│                   │                                                 │
│                   │  ─────────────────────────────────────────────  │
│                   │  ✖ line 1: unexpected token     [ Save Rule ]   │
│ [+ Add Rule]      │                                                 │
│                   │  ▶ API Reference                                │
└───────────────────┴─────────────────────────────────────────────────┘
```

**Rule row status colours:**
- `●` green — enabled, condition currently true (firing)
- `○` dim white — enabled, condition false
- `✖` red — compile error
- `─` grey — disabled by user

**Right panel states:**
- Good script + saved: `✓ Compiled  ● TRUE  Last fired: 0.3s ago  [ Save Rule ]`
- Bad script + saved: `✖ line 1: unexpected token  [ Save Rule ]`
- Unsaved edits: `⚠ Unsaved changes` label appears above Save button

**Empty state (no rule selected):**
```
│              No rule selected.                │
│              [+ Add Rule]                     │
│  ▶ API Reference                              │
```

---

## In-Game Overlay

```
┌─────────────────────┐
│ ● RarePackCast   Q  │
│ ○ UniqueCheck    W  │
│ ✖ BrokenRule     E  │
└─────────────────────┘
```

- Draggable when POEFixer menu is open
- Click-through (`NoInputs | NoMove`) when menu is closed
- Position saved to `settings.json`
- Hidden entirely when `m_Ctx.OverlayVisible` is false

---

## AngelScript API Surface

Users write only the function body. Plugin wraps it before compiling:
```angelscript
// Plugin wraps your body in:
// bool condition(const ConditionState &in s) { <your body> }

// Monster counts:
s.Monsters.Inner.Normal / Magic / Rare / Unique / Total
s.Monsters.Outer.Normal / Magic / Rare / Unique / Total

// Per-monster iteration:
s.Monsters.InnerList.length()
s.Monsters.InnerList[i].Rarity    // 0=Normal 1=Magic 2=Rare 3=Unique
s.Monsters.InnerList[i].Zone      // 1=Inner 2=Outer
s.Monsters.InnerList[i].Path      // entity path string
s.Monsters.InnerList[i].HasBuff("buff_name")
s.Monsters.OuterList[i]           // same fields

// Examples:
// Simple count:
return s.Monsters.Inner.Rare >= 3;

// Unique anywhere nearby:
return s.Monsters.Outer.Unique >= 1;

// HasBuff loop:
for(int i = 0; i < s.Monsters.InnerList.length(); i++) {
    if(s.Monsters.InnerList[i].HasBuff("ignited")) return true;
}
return false;

// Combo condition:
return s.Monsters.Inner.Rare >= 3 && s.Monsters.Outer.Unique >= 1;
```

---

## File + Folder Structure

```
AutoReflex/
│
├── 
│
└── AutoReflex/
    │
    ├── AutoReflex.sln
    ├── AutoReflex.vcxproj
    ├── AutoReflex.h              ← plugin class declaration, owns all subsystems
    ├── AutoReflex.cpp            ← exports + lifecycle, zero logic, pure delegation
    │
    ├── core/
    │   ├── PluginContext.h       ← PluginContext struct: SDK ptr + ALL settings flags
    │   ├── ShouldExecute.h
    │   └── ShouldExecute.cpp     ← bool ShouldExecute(PluginContext*, string& reason)
    │
    ├── game/
    │   ├── MonsterInfo.h         ← MonsterInfo class declaration
    │   ├── MonsterInfo.cpp       ← HasBuff(), AddRef(), Release()
    │   ├── ConditionState.h      ← MonsterCounts, NearbyMonsters, ConditionState
    │   ├── ConditionState.cpp    ← BuildConditionState(PluginContext*)
    │   └── KeySender.h           ← inline PressKey(WORD vk) via SendInput
    │
    ├── scripting/
    │   ├── ScriptEngine.h        ← engine lifecycle + CompileScript + RunCondition
    │   ├── ScriptEngine.cpp
    │   ├── ScriptBindings.h      ← RegisterAllBindings(asIScriptEngine*) declaration
    │   └── ScriptBindings.cpp    ← all RegisterObjectType/Property/Method calls
    │
    ├── rules/
    │   ├── Rule.h                ← Rule struct, pure data, no methods
    │   ├── RuleManager.h         ← vector<Rule>, CompileRule, ExecuteRules, CRUD
    │   └── RuleManager.cpp
    │
    ├── storage/
    │   ├── SettingsStore.h       ← Load/Save PluginContext flags to settings.json
    │   ├── SettingsStore.cpp
    │   ├── RuleStore.h           ← LoadAll, SaveRule, DeleteRule, RenameRule
    │   └── RuleStore.cpp
    │
    ├── ui/
    │   ├── SettingsPanel.h       ← DrawSettings() — top bar + split layout
    │   ├── SettingsPanel.cpp
    │   ├── RuleList.h            ← left panel: rows, context menu, add button
    │   ├── RuleList.cpp
    │   ├── RuleEditor.h          ← right panel: name/key/cooldown/script/save
    │   ├── RuleEditor.cpp
    │   ├── Overlay.h             ← in-game overlay window
    │   ├── Overlay.cpp
    │   └── ApiReference.h        ← header only, static collapsible widget
    │
    ├── lib/
    │   └── json.hpp              ← nlohmann single-header
    │
    └── vendor/
        └── angelscript/
            ├── angelscript.h
            ├── angelscript64.lib
            └── add_on/
                ├── scriptstdstring.cpp/.h
                ├── scriptarray.cpp/.h
                └── scriptbuilder.cpp/.h
```

---

## Include Dependency Rules

| Folder | Can include | Must never include |
|---|---|---|
| `core/` | External SDK only | Nothing internal |
| `game/` | `core/` | `scripting/`, `ui/`, `rules/`, `storage/` |
| `scripting/` | `core/`, `game/` | `ui/`, `storage/` |
| `rules/` | `core/`, `game/`, `scripting/` | `ui/`, `storage/` |
| `storage/` | `core/`, `rules/`, `lib/` | `scripting/`, `ui/` |
| `ui/` | `core/`, `rules/`, `storage/` | `scripting/` directly |

`ui/` never calls AngelScript. It calls `RuleManager::CompileRule()` which calls `ScriptEngine` internally.

---

## Subsystem Initialization Order

```cpp
// OnEnable() — construct in dependency order
m_Ctx.PluginDir    = m_PluginDir;
m_ScriptEngine     = new ScriptEngine(&m_Ctx);
m_ScriptEngine->Init();                             // creates AS engine, registers bindings
m_RuleManager      = new RuleManager(&m_Ctx, m_ScriptEngine);
m_RuleStore        = new RuleStore(&m_Ctx);
m_SettingsStore    = new SettingsStore(&m_Ctx);
m_SettingsPanel    = new SettingsPanel(&m_Ctx, m_RuleManager, m_RuleStore, m_SettingsStore);
m_Overlay          = new Overlay(&m_Ctx, m_RuleManager);
m_SettingsStore->Load();                            // populates m_Ctx flags
m_RuleStore->LoadAll(m_RuleManager);               // loads + compiles all rules

// OnDisable() — destroy in reverse order
m_SettingsStore->Save();
delete m_Overlay;
delete m_SettingsPanel;
delete m_SettingsStore;
delete m_RuleStore;
delete m_RuleManager;
m_ScriptEngine->Shutdown();
delete m_ScriptEngine;
```

---

## Storage Format

### `config/settings.json`
```json
{
  "runInHideout": false,
  "runInTown": false,
  "debugMode": false,
  "overlayVisible": true,
  "overlayX": 100.0,
  "overlayY": 200.0
}
```

### `config/rules/RarePackCast.json`
```json
{
  "name": "RarePackCast",
  "enabled": true,
  "key": 81,
  "cooldownSec": 0.8,
  "order": 0,
  "script": "return s.Monsters.Inner.Rare >= 3;"
}
```

### Save / Load Flow
```
OnEnable()
  EnsureDirectories()
  SettingsStore::Load()       → populates m_Ctx flags
  RuleStore::LoadAll()        → iterates rules/, loads each JSON,
                                sorts by order, calls CompileRule() on each

SaveSettings() callback
  SettingsStore::Save()       → writes settings.json
  for each rule: RuleStore::SaveRule()   → writes rules/<name>.json

Save button clicked
  if name changed:
    RuleStore::RenameRule(oldName, newName)   → filesystem::rename()
    ScriptEngine::DiscardModule(oldName)
  rule.ScriptBody = editBuffer
  CompileRule(rule)           → if fail: rule.Enabled = false
  RuleStore::SaveRule(rule)

Delete rule
  RuleStore::DeleteRule(name)  → filesystem::remove()
  remove from m_Rules
  clamp m_SelectedRule
```

---

## PluginContext Struct

```cpp
// core/PluginContext.h
struct PluginContext {
    // SDK
    PluginSDK::PluginContext* SDK = nullptr;
    std::string               PluginDir;
    std::string               StatusMsg;

    // All plugin settings — no separate struct
    bool  RunInHideout   = false;
    bool  RunInTown      = false;
    bool  DebugMode      = false;
    bool  OverlayVisible = true;
    float OverlayX       = 100.f;
    float OverlayY       = 200.f;
};
```

---

## Rule Struct

```cpp
// rules/Rule.h
struct Rule {
    std::string  Name;
    bool         Enabled      = false;
    WORD         Key          = 0;
    float        CooldownSec  = 1.0f;
    int          Order        = 0;
    std::string  ScriptBody;                          // body only, no boilerplate

    // Runtime — not saved to disk
    asIScriptModule*                          Module       = nullptr;
    std::string                               CompileError;
    bool                                      LastEvalResult = false;
    std::chrono::steady_clock::time_point     LastFired;
    bool                                      EverFired    = false;
};
```

---

## Task List

Each task has a single `OUTPUT` (what exists when it is done) and a `TEST` (how you verify it before moving on). Never move to the next task until the TEST passes.

---

### Phase 1 — Bare Plugin Shell

**Goal: DLL loads, "AutoReflex" appears in POEFixer plugin list, no crash.**

---

**T01 — Create VS solution and DLL project**
- Create VS 2022 solution `AutoReflex.sln`
- Add C++ DLL project `AutoReflex`
- `OUTPUT` Solution file and project file exist
- `TEST` Solution opens in VS with no errors

---

**T02 — Configure build properties**
- Platform: x64, Configuration: Release
- C++ standard: `/std:c++20`
- Runtime library: `/MD`
- Output directory: `$(SolutionDir)x64\Release\Plugins\AutoReflex\`
- `OUTPUT` Build properties set in project
- `TEST` Build produces DLL in the correct output folder

---

**T03 — Add preprocessor defines**
- Add `PLUGIN_EXPORTS` and `_CRT_SECURE_NO_WARNINGS`
- `OUTPUT` Defines present in project properties
- `TEST` Build clean, no warnings about undefined macros

---

**T04 — Add POEFixer SDK include path**
- Add `$(SolutionDir)POEFixer` to include directories
- Create temp `test_include.cpp`, add `#include "plugin_sdk/PluginAPI.h"`, build, delete file
- `OUTPUT` SDK header resolves without error
- `TEST` No "file not found" errors on the include

---

**T05 — Add ImGui source files**
- Add to project: `imgui.cpp`, `imgui_draw.cpp`, `imgui_tables.cpp`, `imgui_widgets.cpp`
- `OUTPUT` All 4 files compile as part of the project
- `TEST` Build succeeds with all 4 ImGui files included

---

**T06 — Create folder structure on disk**
- Create folders: `core/`, `game/`, `scripting/`, `rules/`, `storage/`, `ui/`, `lib/`, `vendor/angelscript/add_on/`
- `OUTPUT` All folders exist in the project directory
- `TEST` Folders visible in VS Solution Explorer after "Show All Files"

---

**T07 — Create `core/PluginContext.h`**
- Define `PluginContext` struct with all fields as specified above
- `OUTPUT` Header file exists and compiles standalone
- `TEST` Include it in a temp cpp, build clean

---

**T08 — Create `AutoReflex.h`**
- Declare `class AutoReflexPlugin : public PluginSDK::IPlugin`
- Include `core/PluginContext.h`
- Declare all IPlugin interface methods as stubs
- Declare all subsystem pointers as `nullptr`
- `OUTPUT` Header compiles with no errors
- `TEST` Include it in `AutoReflex.cpp`, build clean

---

**T09 — Create `AutoReflex.cpp` — exports only**
- Implement `CreatePlugin()` — returns `new AutoReflexPlugin()`
- Implement `DestroyPlugin()` — calls `delete`
- `OUTPUT` DLL built with two exports
- `TEST` `dumpbin /exports AutoReflex.dll` shows `CreatePlugin` and `DestroyPlugin`

---

**T10 — Implement `GetName()` and `GetSDKVersion()`**
- `GetName()` → return `"AutoReflex"`
- `GetSDKVersion()` → return `PLUGIN_SDK_VERSION`
- `OUTPUT` Methods return correct values
- `TEST` Load plugin in POEFixer — name "AutoReflex" appears in plugin list

---

**T11 — Implement `SetContext()` and `SetPluginDirectory()`**
- `SetContext()` → store to `m_Ctx.SDK`, call `ImGui::SetCurrentContext(ctx->ImGuiContext)`
- `SetPluginDirectory()` → store to `m_Ctx.PluginDir`
- `OUTPUT` Both methods implemented
- `TEST` Enable/disable plugin in POEFixer 5 times — no crash

---

**⬛ PHASE 1 COMPLETE** when: AutoReflex appears in plugin list and survives enable/disable without crash.

---

### Phase 2 — Read and Display Game Data

**Goal: see monster zone, rarity, and buff names live in an ImGui window.**

---

**T12 — Verify SDK API method names**
- Open POEFixer SDK headers, confirm exact names for:
  - snapshot access method
  - entity iteration container type
  - zone enum values (InnerCircle / OuterCircle exact names)
  - rarity enum values
  - `ReadBuffsComponent()` return type and buff name field
- `OUTPUT` A comment block at top of `game/ConditionState.h` documenting confirmed API names
- `TEST` Read the comment, confirm all names are verified against actual headers

---

**T13 — Implement `WantsOverlay()`**
- Return `true`
- `OUTPUT` Method returns true
- `TEST` POEFixer shows overlay active for AutoReflex

---

**T14 — Display basic game state**
- In `DrawUI()`, check `m_Ctx.SDK->IsAttached()` — return early if false
- Call `GetSnapshot()`, store result
- Open ImGui window `"AutoReflex##AR"`, display `CurrentState`, `IsTown`, `IsHideout`, `GameWindowForeground`
- `OUTPUT` ImGui window appears in-game with live values
- `TEST` Enter town — `IsTown` shows true. Leave — shows false

---

**T15 — Display monster list**
- Loop `snapshot->Entities` — for each `entityType == Monster`, display `Zone`, `Rarity`, `Path` in a `BeginChild` scrollable area
- `OUTPUT` Monster data visible in debug window
- `TEST` Walk near 3 rares — 3 entries appear with correct zone and rarity values

---

**T16 — Read and display monster buffs**
- For first `InnerCircle` monster where `ComponentCache.HasBuffs()` is true, call `ReadBuffsComponent()` — display each buff name
- `OUTPUT` Buff names print in debug window
- `TEST` At least one buff name appears (e.g. `"monster_base_fire_damage"`)

---

**T17 — Create `core/ShouldExecute.h` and `.cpp`**
- Implement `bool ShouldExecute(PluginContext* ctx, std::string& outReason)`
- Returns false + sets reason for: not attached, not in game, wrong state, not foreground, in town (unless flag set), in hideout (unless flag set)
- `OUTPUT` Function compiles and links
- `TEST` Call it in `DrawUI()`, display result — confirm it returns false in each blocked state

---

**T18 — Add player death check to `ShouldExecute()`**
- Call `GetPlayerVitals()`, if `HPPercent == 0` return false, reason = "Player dead"
- `OUTPUT` Death check added
- `TEST` Die in-game — status shows "Player dead", no keys fire

---

**T19 — Add grace period check to `ShouldExecute()`**
- Loop `vitals.Buffs`, if any `buff.Name == "grace_period"` return false, reason = "Grace period"
- `OUTPUT` Grace period check added
- `TEST` Enter new area — status shows "Grace period" for first few seconds

---

**T20 — Display `StatusMsg` in debug window**
- Show `m_Ctx.StatusMsg` at top of debug window with colour: green = active, yellow = warning, red = blocked
- `OUTPUT` Status message visible and colour-coded
- `TEST` All 5 block reasons show correct message and colour

---

**⬛ PHASE 2 COMPLETE** when: monster list, rarity, zone, and buffs are all visible live in-game.

---

### Phase 3 — Key Press and Cooldown

**Goal: a key fires at correct interval, no spam, logged each time.**

---

**T21 — Create `game/KeySender.h`**
- Inline function `void PressKey(WORD vk)` using `SendInput` keydown + keyup pair
- `OUTPUT` Header file exists, function compiles
- `TEST` Call `PressKey('Q')` once directly — Q fires in-game exactly once

---

**T22 — Add cooldown to test fire**
- Add `m_LastTestFire` (`chrono::steady_clock::time_point`) to plugin
- Call `PressKey('Q')` only when 800ms elapsed since last fire
- `OUTPUT` Q fires at ~800ms intervals
- `TEST` Watch in-game — Q fires at correct interval, not every frame

---

**T23 — Create debug log**
- Add `m_DebugLog` as `std::vector<std::string>`, cap at 200 entries
- Add `Log(const std::string& msg)` helper
- Add scrolling `BeginChild` in debug window, auto-scroll on new entry
- `OUTPUT` Log window visible with scrolling
- `TEST` Add 201 entries — only 200 kept. New entries auto-scroll to bottom

---

**T24 — Log each test keypress**
- Call `Log("Pressed Q — " + timestamp)` on each fire
- `OUTPUT` Each fire produces exactly one log entry
- `TEST` Count log entries over 5 seconds at 800ms interval — should be ~6 entries

---

**T25 — Remove hardcoded test fire**
- Remove the Q press and test cooldown from `DrawUI()`
- Replace with comment `// Rule execution goes here — Phase 7`
- `OUTPUT` No keys fire
- `TEST` Confirm no Q presses, no log entries — clean slate

---

**⬛ PHASE 3 COMPLETE** when: PressKey works, cooldown is correct, log works, hardcoded test removed.

---

### Phase 4 — AngelScript Engine

**Goal: a script string compiles, runs, returns a bool, errors surface with line numbers.**

---

**T26 — Add AngelScript to project**
- Add `vendor/angelscript/angelscript.h` to includes
- Add `angelscript64.lib` to linker
- Add to project: `scriptstdstring.cpp`, `scriptarray.cpp`, `scriptbuilder.cpp`
- `OUTPUT` All AngelScript files compile as part of project
- `TEST` Build succeeds, no linker errors

---

**T27 — Create `scripting/ScriptEngine.h` and `.cpp`**
- Declare class `ScriptEngine` with constructor `ScriptEngine(PluginContext* ctx)`
- Declare `Init()`, `Shutdown()`
- `OUTPUT` Class declaration compiles
- `TEST` Header includable without errors

---

**T28 — Implement `ScriptEngine::Init()`**
- Call `asCreateScriptEngine()`, store as `m_Engine`
- Register message callback → calls `m_Ctx->StatusMsg = msg` and `Log(msg)`
- Call `RegisterStdString(m_Engine)` and `RegisterScriptArray(m_Engine, true)`
- `OUTPUT` Engine created and string/array support registered
- `TEST` Enable/disable plugin 5 times — no crash, no leak visible in task manager

---

**T29 — Implement `ScriptEngine::Shutdown()`**
- Call `m_Engine->ShutDownAndRelease()`, set `m_Engine = nullptr`
- `OUTPUT` Engine cleaned up on disable
- `TEST` Disable plugin — no crash

---

**T30 — Implement `ScriptEngine::CompileScript()`**
- Signature: `asIScriptModule* CompileScript(const std::string& moduleName, const std::string& body, std::string& outError)`
- Wraps body: `"bool condition(const ConditionState &in s) { " + body + " }"`
- Uses `CScriptBuilder` — returns module pointer or null, sets `outError` on failure
- `OUTPUT` Function compiles and links
- `TEST` Pass `"return true;"` — returns non-null, `outError` empty

---

**T31 — Implement `ScriptEngine::RunCondition()`**
- Signature: `bool RunCondition(asIScriptModule* mod, ConditionState& state)`
- Gets function `bool condition(const ConditionState &in s)`, creates context, executes, returns bool
- `OUTPUT` Function compiles and links
- `TEST` Module from T30 returns `true` when called

---

**T32 — Test false return**
- Hardcode compile `"return false;"` in `OnEnable()`, call each frame, log result
- `OUTPUT` Log shows `false` every frame
- `TEST` Confirm log shows `false`, not `true`

---

**T33 — Test compile error**
- Change body to `"return"` (missing semicolon)
- `OUTPUT` `outError` is non-empty with line number
- `TEST` Error text contains `"line 1"` reference

---

**T34 — Display compile error in UI**
- Show `outError` text in red in debug window
- `OUTPUT` Red error text visible in window
- `TEST` Bad script → red error. Fix script → error disappears

---

**T35 — Remove hardcoded test compile**
- Remove test from `OnEnable()`
- `OUTPUT` No scripts compile on startup
- `TEST` No errors shown, no log output on startup

---

**⬛ PHASE 4 COMPLETE** when: scripts compile, run, return correct bool, errors show with line numbers.

---

### Phase 5 — ConditionState: Monster Counts

**Goal: a script can read `s.Monsters.Inner.Rare` and get the correct live value.**

---

**T36 — Create `game/ConditionState.h`**
- Define `MonsterCounts` struct: `int Normal, Magic, Rare, Unique, Total`
- Define `NearbyMonsters` struct: `MonsterCounts Inner, Outer` + `CScriptArray* InnerList, OuterList` (initialized null)
- Define `ConditionState` struct: `NearbyMonsters Monsters`
- `OUTPUT` Header compiles standalone
- `TEST` Include in a temp file, zero-initialize a `ConditionState` — no crash

---

**T37 — Implement `BuildConditionState()` in `game/ConditionState.cpp`**
- Signature: `ConditionState BuildConditionState(PluginContext* ctx)`
- Loop `snapshot->Entities` once, count monsters by zone + rarity into Inner/Outer
- Return populated `ConditionState` (InnerList/OuterList left null for now — filled in Phase 6)
- `OUTPUT` Function returns correct counts
- `TEST` Display all 8 count values in debug window — walk near 2 rares, confirm `Inner.Rare == 2`

---

**T38 — Create `scripting/ScriptBindings.h` and `.cpp`**
- Declare `void RegisterAllBindings(asIScriptEngine* engine)`
- `OUTPUT` Files exist and compile
- `TEST` Header includable without errors

---

**T39 — Register `MonsterCounts` with AngelScript**
- In `RegisterAllBindings()`: `RegisterObjectType("MonsterCounts", sizeof(MonsterCounts), asOBJ_VALUE | asOBJ_POD)`
- Register all 5 int properties
- `OUTPUT` Type registered
- `TEST` No registration error codes returned (check return values)

---

**T40 — Register `NearbyMonsters` as reference type**
- Register as reference type (not value — it contains CScriptArray pointers)
- Register `Inner` and `Outer` as `MonsterCounts` properties
- Register `AddRef` and `Release` behaviours
- `OUTPUT` Type registered without errors
- `TEST` No registration errors

---

**T41 — Register `ConditionState` as reference type**
- Register `Monsters` as `NearbyMonsters@` property
- `OUTPUT` Type registered without errors
- `TEST` No registration errors

---

**T42 — Call `RegisterAllBindings()` in `ScriptEngine::Init()`**
- `OUTPUT` Bindings registered on engine startup
- `TEST` Build clean, no registration errors in log on enable

---

**T43 — Update `RunCondition()` to pass `ConditionState`**
- Pass `ConditionState` via `SetArgObject`
- `OUTPUT` Signature update compiles
- `TEST` Build succeeds

---

**T44 — Live count test**
- Hardcode: compile `"return s.Monsters.Inner.Rare >= 1;"`, call with live state, log result
- `OUTPUT` Result reflects actual monster count
- `TEST` Walk near a rare — log shows `true`. Move away — shows `false`

---

**T45 — Test boundary values**
- Test `"return s.Monsters.Outer.Total >= 5;"` — confirm changes with monster count
- Test wrong field `"return s.Monsters.Inner.Legendary >= 1;"` — confirm compile error
- `OUTPUT` Correct results and correct error message
- `TEST` Both behaviours verified

---

**T46 — Remove hardcoded count test**
- `OUTPUT` No hardcoded scripts
- `TEST` No log output, no errors on startup

---

**⬛ PHASE 5 COMPLETE** when: scripts can read all 8 count values and return correct live results.

---

### Phase 6 — ConditionState: Per-Monster Buff Checking

**Goal: a script can iterate InnerList and call HasBuff() on each monster.**

---

**T47 — Create `game/MonsterInfo.h`**
- Declare class `MonsterInfo`:
  - Public: `int Rarity`, `int Zone`, `std::string Path`
  - Public: `bool HasBuff(const std::string& name)`
  - Public: `void AddRef()`, `void Release()`
  - Private: `int m_RefCount`, `uintptr_t m_BuffsAddr`, `bool m_HasBuffsComp`, `PluginContext* m_Ctx`
- `OUTPUT` Header compiles
- `TEST` Include in temp file, declare a pointer — no errors

---

**T48 — Implement `MonsterInfo.cpp`**
- `AddRef()` → `m_RefCount++`
- `Release()` → `if(--m_RefCount == 0) delete this`
- `HasBuff()` → if `!m_HasBuffsComp` return false; else call `ReadBuffsComponent(m_BuffsAddr)`, search names, return bool
- `OUTPUT` All methods implemented
- `TEST` Call `HasBuff()` directly from C++ on a known entity — returns correct true/false

---

**T49 — Register `MonsterInfo` with AngelScript**
- Register as reference type with `AddRef`/`Release` behaviours
- Register `Rarity`, `Zone`, `Path` as properties
- Register `HasBuff(const string &in)` as method
- `OUTPUT` Type registered without errors
- `TEST` Script `"return s.Monsters.InnerList[0].Rarity >= 2;"` compiles without error

---

**T50 — Build `InnerList` in `BuildConditionState()`**
- Create `CScriptArray` with type `MonsterInfo@`
- For each `InnerCircle` monster: create `new MonsterInfo(...)`, populate all fields, push to array
- Assign to `state.Monsters.InnerList`
- `OUTPUT` InnerList populated with correct monster objects
- `TEST` Script logs `s.Monsters.InnerList.length()` — matches expected inner monster count

---

**T51 — Register `InnerList` on `NearbyMonsters`**
- Register `CScriptArray* InnerList` as property
- `OUTPUT` Property accessible from script
- `TEST` `s.Monsters.InnerList.length()` runs without error

---

**T52 — Test HasBuff iteration**
- Hardcode and test:
  ```angelscript
  for(int i=0; i<s.Monsters.InnerList.length(); i++) {
      if(s.Monsters.InnerList[i].HasBuff("ignited")) return true;
  }
  return false;
  ```
- `OUTPUT` Script runs without crash
- `TEST` Apply ignite to nearby monster — returns `true`. Without ignite — returns `false`

---

**T53 — Test empty InnerList**
- Stand in area with no monsters, run script
- `OUTPUT` `length()` returns 0
- `TEST` No crash, returns `false`

---

**T54 — Test HasBuff on entity with no buffs component**
- Find entity where `HasBuffs()` returns false, call `HasBuff()` on it
- `OUTPUT` Returns false without crash
- `TEST` No crash, no access violation

---

**T55 — Build `OuterList` in `BuildConditionState()`**
- Same pattern as T50 for `OuterCircle` monsters
- Register `OuterList` on `NearbyMonsters`
- `OUTPUT` OuterList populated and accessible
- `TEST` Script `"return s.Monsters.OuterList.length() >= 3;"` returns correct result

---

**T56 — Remove hardcoded buff test**
- `OUTPUT` No hardcoded scripts
- `TEST` Clean startup, no errors

---

**⬛ PHASE 6 COMPLETE** when: scripts can iterate InnerList/OuterList and call HasBuff() correctly.

---

### Phase 7 — Rule and RuleManager

**Goal: multiple independent rules each fire at their own interval with their own key.**

---

**T57 — Create `rules/Rule.h`**
- Define `Rule` struct with all fields as specified above
- `OUTPUT` Header compiles
- `TEST` Include in temp file, default-construct a `Rule` — no errors

---

**T58 — Create `rules/RuleManager.h` and `.cpp`**
- Class `RuleManager(PluginContext* ctx, ScriptEngine* engine)`
- Members: `std::vector<Rule> m_Rules`, `int m_SelectedRule = -1`
- `OUTPUT` Class compiles
- `TEST` Construct in `OnEnable()` — no crash

---

**T59 — Implement `RuleManager::CompileRule()`**
- Wraps `r.ScriptBody` in boilerplate, calls `ScriptEngine::CompileScript()`
- On fail: sets `r.CompileError`, forces `r.Enabled = false`
- On success: clears `r.CompileError`, sets `r.Module`
- `OUTPUT` Function compiles and links
- `TEST` Rule with valid body: Module non-null, error empty. Invalid body: Module null, error set, Enabled forced false

---

**T60 — Implement `RuleManager::ExecuteRules()`**
- Call `BuildConditionState()` once
- Loop enabled rules with non-null Module
- Call `RunCondition()`, store in `r.LastEvalResult`
- If true and cooldown elapsed: `PressKey(r.Key)`, update `r.LastFired`, `r.EverFired = true`, `Log()`
- `OUTPUT` Rules execute correctly
- `TEST` Add rule manually in `OnEnable()` with body `"return true;"` — confirm key fires at correct interval

---

**T61 — Call `ExecuteRules()` from `DrawUI()`**
- Only when `ShouldExecute()` returns true
- `OUTPUT` Rules execute in game only when appropriate
- `TEST` Enter town — no firing. Leave town — firing resumes

---

**T62 — Implement `CreateDefaultRules()`**
- Creates one rule: name `"RarePackCast"`, key `'Q'`, cooldown `0.8f`, body `"return s.Monsters.Inner.Rare >= 3;"`
- Called in `OnEnable()` only if `m_Rules` is empty after loading
- `OUTPUT` Default rule created on first run
- `TEST` Fresh install — default rule appears, compiles, fires Q when 3+ rares in inner circle

---

**T63 — Test two rules simultaneously**
- Manually add second rule: key `'W'`, cooldown `1.5f`, body `"return s.Monsters.Outer.Unique >= 1;"`
- `OUTPUT` Both rules fire independently
- `TEST` Each key fires at its own interval, neither interferes with the other

---

**⬛ PHASE 7 COMPLETE** when: multiple rules execute independently with correct keys and cooldowns.

---

### Phase 8 — File Storage

**Goal: rules and settings survive plugin reload and POEFixer restart.**

---

**T64 — Create `storage/SettingsStore.h` and `.cpp`**
- Class `SettingsStore(PluginContext* ctx)`
- `Load()` → reads `config/settings.json`, populates `m_Ctx` flags. If missing, uses defaults.
- `Save()` → writes all `m_Ctx` flags to `config/settings.json`
- `OUTPUT` Class compiles
- `TEST` Change `RunInHideout` to true, call `Save()`, open file in text editor — `"runInHideout": true`

---

**T65 — Create `storage/RuleStore.h` and `.cpp`**
- Class `RuleStore(PluginContext* ctx)`
- `EnsureDirectories()` → creates `config/` and `config/rules/` via `filesystem::create_directories()`
- `OUTPUT` Class and method compile
- `TEST` Call `EnsureDirectories()` — folders appear on disk

---

**T66 — Implement `RuleStore::SaveRule()`**
- Serializes all `Rule` fields (excluding runtime fields) to `config/rules/<name>.json`
- `OUTPUT` File written to disk
- `TEST` Save a rule, open file — all fields present and correct including script body

---

**T67 — Implement `RuleStore::LoadRule()`**
- Reads JSON file, populates `Rule` struct — does NOT compile
- Returns `Rule` by value
- `OUTPUT` Rule loaded from disk
- `TEST` Load the file saved in T66 — all fields match original

---

**T68 — Implement `RuleStore::LoadAll()`**
- `LoadAll(RuleManager* rm)` → iterates `config/rules/*.json`, loads each, sorts by `order`, calls `rm->CompileRule()` on each, pushes to `rm->m_Rules`
- `OUTPUT` All rules loaded and compiled
- `TEST` Create two JSON files manually, call `LoadAll()` — both appear in `m_Rules`, compiled

---

**T69 — Implement `RuleStore::DeleteRule()`**
- `DeleteRule(const std::string& name)` → `filesystem::remove("config/rules/" + name + ".json")`
- `OUTPUT` File deleted
- `TEST` Save a rule, call `DeleteRule()`, confirm file gone from disk

---

**T70 — Implement `RuleStore::RenameRule()`**
- `RenameRule(const std::string& oldName, const std::string& newName)` → `filesystem::rename()`
- `OUTPUT` Old file gone, new file present with same content
- `TEST` Rename `"RuleA"` to `"RuleB"` — `RuleA.json` gone, `RuleB.json` exists

---

**T71 — Wire `LoadSettings()` and `LoadAll()` into `OnEnable()`**
- Call `EnsureDirectories()`, `SettingsStore::Load()`, `RuleStore::LoadAll()`
- `OUTPUT` Full load on enable
- `TEST` Create rules, disable plugin, re-enable — all rules reloaded and compiled

---

**T72 — Wire `Save()` into `SaveSettings()` override**
- Call `SettingsStore::Save()`, loop rules calling `RuleStore::SaveRule()`
- `OUTPUT` Full save on POEFixer save callback
- `TEST` Change a flag and rule, trigger save, restart — changes persist

---

**T73 — Test persistence of compile errors**
- Save a rule with bad script, restart plugin
- `OUTPUT` Rule reloads with error preserved
- `TEST` Error visible in UI, rule force-disabled — no crash

---

**⬛ PHASE 8 COMPLETE** when: all rules and settings survive a full POEFixer restart.

---

### Phase 9 — Settings Panel UI

**Goal: full create/edit/delete workflow in ImGui without touching files manually.**

---

**T74 — Verify POEFixer settings panel width**
- Open POEFixer, measure available pixel width in settings panel
- `OUTPUT` Width value noted
- `TEST` If >= 500px: proceed with split panel. If < 500px: switch to stacked layout before T75

---

**T75 — Create `ui/SettingsPanel.h` and `.cpp`**
- Class `SettingsPanel(PluginContext*, RuleManager*, RuleStore*, SettingsStore*)`
- `DrawSettings()` method declared
- `OUTPUT` Class compiles
- `TEST` Call `DrawSettings()` from `AutoReflex.cpp::DrawSettings()` — no crash

---

**T76 — Render top bar**
- Three `ImGui::Checkbox` on one row: `Run in Hideout`, `Run in Town`, `Debug Mode`
- Status label right-aligned: `m_Ctx->StatusMsg` in colour
- `OUTPUT` Top bar visible
- `TEST` Toggle each checkbox — value updates in `m_Ctx`, confirmed by checking flag in debugger

---

**T77 — Create `ui/RuleList.h` and `.cpp`**
- Class `RuleList(PluginContext*, RuleManager*, RuleStore*)`
- `Draw()` method declared
- `OUTPUT` Class compiles
- `TEST` Call `Draw()` — no crash even with empty rule list

---

**T78 — Render left panel container**
- `BeginChild("##ARList", ImVec2(200, 0))` for fixed-width left panel
- `OUTPUT` Left panel container visible
- `TEST` Left panel 200px wide, right panel takes remaining width

---

**T79 — Render rule rows**
- One row per rule: coloured status dot, name, key label right-aligned
- `OUTPUT` All rules appear as rows
- `TEST` 3 rules visible with correct names, keys, and status colours

---

**T80 — Implement row selection**
- Click row → `m_RuleManager->m_SelectedRule = i`
- Highlight selected row with 3px left accent bar via `ImDrawList::AddRectFilled`
- `OUTPUT` Selection visual works
- `TEST` Click each row — teal accent bar moves to selected row

---

**T81 — Status dot colours**
- Green `●` if enabled and `LastEvalResult == true`
- Dim `○` if enabled and `LastEvalResult == false`
- Red `✖` if `CompileError` non-empty`
- Grey `─` if `!Enabled`
- `OUTPUT` All 4 states render correctly
- `TEST` Create 4 rules covering each state — all show correct colour

---

**T82 — Right-click context menu**
- `BeginPopupContextItem` on row — items: `Clone`, `Move Up`, `Move Down`, `Delete`
- `OUTPUT` Context menu appears on right-click
- `TEST` Right-click each row — menu appears with 4 items

---

**T83 — Implement Clone**
- Deep copy rule struct, append `"_copy"` to name, set order = last, call `CompileRule()`, call `SaveRule()`
- `OUTPUT` Clone appears in list
- `TEST` Clone a rule — new rule appears with `_copy` suffix, same script, independent from original

---

**T84 — Implement Move Up / Move Down**
- Swap `order` values of adjacent rules, re-sort `m_Rules`, save both affected JSONs
- `OUTPUT` Order changes persist
- `TEST` Move a rule up — list reorders. Restart plugin — order preserved

---

**T85 — Implement Delete**
- Remove from `m_Rules`, call `RuleStore::DeleteRule()`, clamp `m_SelectedRule`
- `OUTPUT` Rule removed from list and disk
- `TEST` Delete a rule — gone from list, JSON deleted from disk

---

**T86 — Render `[+ Add Rule]` button**
- Bottom of left panel
- Click: create `Rule` with defaults (enabled=false, no key, empty body, order=last), push to `m_Rules`, call `SaveRule()`, set `m_SelectedRule` to new rule, auto-focus name field
- `OUTPUT` Button visible and functional
- `TEST` Click button — new rule appears at bottom, right panel opens, name field focused

---

**T87 — Create `ui/RuleEditor.h` and `.cpp`**
- Class `RuleEditor(PluginContext*, RuleManager*, RuleStore*)`
- `Draw()` method declared
- Owns `m_EditNameBuffer[256]`, `m_EditScriptBuffer[4096]`, `m_HasUnsavedChanges` bool
- `OUTPUT` Class compiles
- `TEST` Construct and call `Draw()` with `m_SelectedRule == -1` — no crash

---

**T88 — Render empty state**
- When `m_SelectedRule == -1`: centered "No rule selected." text, centered `[+ Add Rule]` button, collapsed API Reference
- `OUTPUT` Empty state renders correctly
- `TEST` Start with no selection — empty state visible. Click Add Rule — switches to editor

---

**T89 — Populate edit buffers on selection change**
- When `m_SelectedRule` changes: copy `rule.Name` to `m_EditNameBuffer`, `rule.ScriptBody` to `m_EditScriptBuffer`, set `m_HasUnsavedChanges = false`
- `OUTPUT` Buffers populated on selection
- `TEST` Select rule A — buffers show A's data. Select rule B — buffers switch to B's data

---

**T90 — Render Name field**
- `InputText` bound to `m_EditNameBuffer`
- On any change: set `m_HasUnsavedChanges = true`
- Turn red (`PushStyleColor`) if name duplicates another rule or contains invalid chars `/ \ : * ? " < > |`
- Show inline error text `"Name already exists"` or `"Invalid character"` below field
- `OUTPUT` Name field functional with validation
- `TEST` Type duplicate name — field red, error text appears. Type unique name — field normal

---

**T91 — Render Key dropdown**
- `ImGui::Combo` with list: `"None","1","2","3","4","5","Q","W","E","R","F1"..."F12"`
- Maps to VK codes: `0, '1','2','3','4','5', 'Q','W','E','R', VK_F1...VK_F12`
- On change: set `m_HasUnsavedChanges = true`
- `OUTPUT` Dropdown functional
- `TEST` Select Q — rule key is `0x51`. Select F1 — rule key is `VK_F1`

---

**T92 — Render Cooldown drag**
- `DragFloat("##cd", &r.CooldownSec, 0.05f, 0.1f, 10.0f, "%.2f s")`
- On change: set `m_HasUnsavedChanges = true`
- `OUTPUT` Drag functional, clamped
- `TEST` Drag below 0.1 — stays at 0.1. Drag above 10.0 — stays at 10.0

---

**T93 — Render Enabled checkbox**
- If `CompileError` non-empty: `BeginDisabled()` wrapper, checkbox greyed out
- `OUTPUT` Checkbox disabled when script has error
- `TEST` Rule with error — checkbox greyed, cannot enable. Fix error and save — checkbox active

---

**T94 — Render script editor**
- `InputTextMultiline` bound to `m_EditScriptBuffer`
- On any change: set `m_HasUnsavedChanges = true`
- `OUTPUT` Script editor functional
- `TEST` Edit script — `m_HasUnsavedChanges` becomes true

---

**T95 — Render unsaved changes warning**
- When `m_HasUnsavedChanges`: show `"⚠ Unsaved changes"` label in yellow above Save button
- When user clicks a different rule while `m_HasUnsavedChanges`: silently discard, switch selection
- `OUTPUT` Warning visible when unsaved changes exist
- `TEST` Edit script, click different rule — warning disappears, other rule loads

---

**T96 — Render Save button**
- Disabled (grey via `BeginDisabled`) when: name is empty, name is duplicate, name has invalid chars
- On click:
  - If name changed: call `RuleStore::RenameRule()`, `ScriptEngine::DiscardModule(oldName)`
  - Copy buffers to rule fields
  - Call `RuleManager::CompileRule(rule)`
  - Call `RuleStore::SaveRule(rule)`
  - Set `m_HasUnsavedChanges = false`
- `OUTPUT` Save button functional with all guards
- `TEST` Edit name + script, click Save — file renamed/updated on disk, compile result shown

---

**T97 — Render compile status**
- Below Save button: green `✓ Compiled` or red `✖ <error text>`
- `OUTPUT` Status updates after Save click
- `TEST` Save good script — green. Save bad script — red with error message

---

**T98 — Render live eval result**
- Next to compile status: green `● TRUE` or dim `○ false` (updates every frame from `r.LastEvalResult`)
- `OUTPUT` Eval result updates live
- `TEST` Walk near rares — dot turns green when condition met

---

**T99 — Create `ui/ApiReference.h`**
- Header-only `void DrawApiReference()` function
- Renders `CollapsingHeader("▶ API Reference")` — when expanded shows full API as `TextDisabled` lines
- Quick Insert buttons: `[Rare Pack]`, `[Unique Present]`, `[HasBuff Loop]`
- Each button takes a `char* buffer, size_t size` and appends the snippet
- `OUTPUT` Widget renders correctly
- `TEST` Collapsed by default. Expand — API text visible. Click `[Rare Pack]` — snippet appended to edit buffer

---

**⬛ PHASE 9 COMPLETE** when: full rule create/edit/delete/reorder workflow works entirely in ImGui.

---

### Phase 10 — In-Game Overlay

**Goal: small always-visible status window, draggable when menu open, click-through when closed.**

---

**T100 — Create `ui/Overlay.h` and `.cpp`**
- Class `Overlay(PluginContext* ctx, RuleManager* rm)`
- `Draw()` method
- `OUTPUT` Class compiles
- `TEST` Call `Draw()` — no crash

---

**T101 — Render overlay window**
- `ImGui::Begin("##AROverlay", nullptr, NoTitleBar | NoResize | AlwaysAutoResize | NoScrollbar | NoCollapse)`
- Set initial position from `m_Ctx->OverlayX/Y`
- One row per rule: status dot, name, key char
- `OUTPUT` Overlay window visible in-game
- `TEST` All rules appear with correct names and status dots

---

**T102 — Implement click-through and drag**
- When `IsMenuVisible()` true: normal window, drag allowed, save position to `m_Ctx->OverlayX/Y`
- When `IsMenuVisible()` false: add `NoInputs | NoMove` flags
- `OUTPUT` Drag and click-through behaviour correct
- `TEST` Open menu — drag overlay to new position. Close menu — overlay stays, click-through (game clicks pass through)

---

**T103 — Persist overlay position**
- `m_Ctx->OverlayX/Y` already saved with `SettingsStore::Save()` and loaded with `Load()`
- `OUTPUT` Position persists
- `TEST` Move overlay, trigger save, restart POEFixer — overlay appears at saved position

---

**T104 — Add overlay toggle**
- `[✓] Show Overlay` checkbox in top bar of settings panel
- Toggles `m_Ctx->OverlayVisible` — overlay only renders when true
- `OUTPUT` Toggle functional
- `TEST` Uncheck — overlay disappears. Check — overlay appears

---

**⬛ PHASE 10 COMPLETE** when: overlay shows live rule states, is draggable in menu, click-through in game.

---

### Phase 11 — Area Change and Edge Cases

**Goal: no stale data, no phantom entities, all edge cases handled cleanly.**

---

**T105 — Track area change counter**
- Store `m_LastAreaChangeCounter` in plugin
- Compare to `snapshot->AreaChangeCounter` each frame
- On change: `Log("Area changed")`, reset any per-area flags
- `OUTPUT` Area changes detected
- `TEST` Move between two zones — exactly one log entry per transition

---

**T106 — Confirm monster list resets on area change**
- `MonsterInfo` objects in InnerList/OuterList are rebuilt from scratch in `BuildConditionState()` every frame
- Verify no entity pointers from previous area persist
- `OUTPUT` Confirmed by code review
- `TEST` Move zones, stand still — monster list contains only current area entities, count is correct

---

**T107 — Test all `ShouldExecute()` block cases**
- In town (flag off), in hideout (flag off), dead, grace period, not foreground
- `OUTPUT` Each case blocks correctly
- `TEST` Each block reason shows correct `StatusMsg`, no keys fire in any blocked state

---

**T108 — Test rapid Save clicks**
- Click Save 10 times quickly on same rule
- `OUTPUT` One file on disk, correct content
- `TEST` No duplicate files, no crash, no corrupted JSON

---

**T109 — Test enable/disable cycle**
- Enable plugin → disable → enable, repeat 5 times
- `OUTPUT` Plugin survives repeated enable/disable
- `TEST` No crash, no memory leak (check task manager), all rules reload correctly each time

---

**T110 — Test 5 rules simultaneously**
- Create 5 rules with different cooldowns (0.3s, 0.5s, 0.8s, 1.2s, 2.0s) all with `"return true;"`
- `OUTPUT` All 5 fire independently
- `TEST` Log shows all 5 firing at their own intervals, none interfere

---

**T111 — Test script with AngelScript runtime error**
- Write a script that compiles but throws at runtime (e.g. array out-of-bounds access)
- `OUTPUT` Runtime error caught, logged, rule does not fire
- `TEST` No crash, error visible in log, other rules unaffected

---

**⬛ PHASE 11 COMPLETE** when: all edge cases handled cleanly, no crashes in any tested scenario.

---

### Phase 12 — Polish

**Goal: production-ready, zero rough edges.**

---

**T112 — Add cooldown progress bar per rule in left panel**
- Thin `ProgressBar` under each rule row showing time remaining until next fire (0.0 to 1.0)
- `OUTPUT` Progress bar visible and animating
- `TEST` Bar drains to zero and resets on each fire

---

**T113 — Add "Last fired" display in rule editor**
- Status bar shows `"Last fired: 0.3s ago"` or `"never"` if `!r.EverFired`
- `OUTPUT` Time since last fire displayed
- `TEST` Fires once — shows `"0.0s ago"`. Wait 5s — shows `"5.0s ago"`

---

**T114 — Add `[Clear Log]` button**
- In debug window (visible when `DebugMode` is true)
- Clears `m_DebugLog`
- `OUTPUT` Button visible, functional
- `TEST` Click — all log entries cleared

---

**T115 — Rename `ScriptEngine::DiscardModule()` flow**
- In Save button handler: before calling `CompileScript` with new name, call `m_Engine->DiscardModule(oldModuleName.c_str())`
- `OUTPUT` Old module cleaned up from AS engine on rename
- `TEST` Rename a rule 5 times — no memory growth in AS engine

---

**T116 — Final Release build**
- Build in Release x64, check DLL size, confirm no debug symbols in output
- Run full test: cold start, load rules, fire keys, save, restart, reload
- `OUTPUT` Release DLL built clean
- `TEST` Cold start — plugin loads, rules compile, first fire happens correctly, no console errors

---

**⬛ PHASE 12 COMPLETE. AUTOREFLEX IS DONE.**

---

## Task Summary

| Phase | Tasks | Deliverable |
|---|---|---|
| 1 — Shell | T01–T11 | DLL loads in POEFixer |
| 2 — Data | T12–T20 | Monster data visible live |
| 3 — Keys | T21–T25 | Keys fire at correct interval |
| 4 — AngelScript | T26–T35 | Scripts compile and run |
| 5 — Monster counts | T36–T46 | Scripts read count data |
| 6 — Monster buffs | T47–T56 | Scripts check per-monster buffs |
| 7 — Rules | T57–T63 | Multiple independent rules |
| 8 — Storage | T64–T73 | Rules persist to disk |
| 9 — Settings UI | T74–T99 | Full edit workflow in ImGui |
| 10 — Overlay | T100–T104 | In-game status window |
| 11 — Edge cases | T105–T111 | No silent failures |
| 12 — Polish | T112–T116 | Production ready |
| **Total** | **116 tasks** | |

## Critical Path

```
T01→T11 → T12→T20 → T21→T25 → T26→T35 → T36→T46 → T47→T56 → T57→T63
```

**Plugin fires real keys based on real monster conditions at T63.**
Everything after T63 is persistence (T64–T73), UI (T74–T104), and polish (T105–T116).
