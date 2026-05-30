# AutoReflex Internals (Design + Intent)

This document explains **how AutoReflex works internally**, and **why** the key functions exist.
It is written for future maintenance (performance + correctness) and aims to match the current code.

Constraints / invariants we intentionally keep:

- **Never modify `sdk/` or `imgui/`** in this repo (keep parity with upstream POEFixer/ExamplePlugin).
- Hot path goals: **cheap filters first**, defer `GetSnapshot()` until needed, avoid repeated host calls across rules on the same tick.

---

## High-level pipeline

AutoReflex runs as a POEFixer plugin (SDK v6). The host calls `AutoReflexPlugin::DrawUI()` every frame.

The plugin uses a **two-stage gate** before doing any expensive work:

1. **Cheap gates (no snapshot)** — every frame:
   - `HostCompatible()` / `ctx()` valid
   - `Game.IsAttached()`, `IsInGame()`, `IsForeground()`
   - `AnimationLock` not active (`WaitAfterPressMs` after a keypress)
2. **Eval tick** — after cheap gates pass:
   - `Game.GetSnapshot()` once per `DrawUI()` call (cadence follows the host's game data poll)
   - `EvalTickCache::BeginTick()` — mouse position, player grace buff, per-tick W2S/buff caches
   - `DetermineWhetherRulesShouldExecute(...)` — town/hideout/pause/skill tree/dead/grace
   - `RuleManager::EvaluateRulesAgainstSnapshotUntilFirstFire(...)`

Important behavior decisions:

- **One fire per evaluation tick**: once any rule triggers, remaining rules are skipped for that tick.
- Rule evaluation cadence is **not** throttled inside the plugin — POEFixer controls snapshot freshness via host Performance settings.
- **Animation lock** honors `WaitAfterPressMs` after each keypress (blocks eval until elapsed).

---

## Key files and responsibilities

- `AutoReflex.cpp` / `AutoReflex.h`
  - plugin lifecycle (`OnEnable`, `OnDisable`, `DrawUI`, `DrawSettings`)
  - cheap pre-snapshot gates, animation lock, event subscriptions
  - `OnAreaChange` → reset animation lock; `OnGameDetached` → save settings

- `core/AnimationLock.h` — post-keypress wait (`WaitAfterPressMs`)
- `core/EvalTickCache.h` — per-tick shared caches (mouse, W2S, buff reads, grace period)
- `core/ShouldExecute.cpp` — snapshot-level gates (town, pause, skill tree, dead, grace)

- `rules/RuleManager.cpp`
  - loads rules, compiles them, evaluates in `Order`
  - builds hostile/friendly candidate lists **only when enabled rules need them**
  - engages animation lock when a rule fires

- `scripting/ScriptEngine.{h,cpp}`
  - `CompiledExpression`: compile + evaluate via EXPRTK
  - DSL preprocessing via `ScriptEngineDslPreprocessor`
  - uses `EvalTickCache` for cursor distance and buff reads across rules on the same tick

---

## Rule structure and lifecycle

### `Rule` (what it means)

From `rules/Rule.h`:

- **User fields (persisted)**: `Name`, `Enabled`, `Key`, `CooldownSec`, `WaitAfterPressMs`, `Order`, `ScriptBody`
- **Runtime fields**: `CompiledExpr`, `CompileError`, `LastEvalResult`, `LastFired`, `EverFired`, `Root`

### `RuleManager::EvaluateRulesAgainstSnapshotUntilFirstFire(...)`

Hot-path structure:

1. Scan enabled rules to decide whether to build hostile and/or friendly candidate lists.
2. Build candidate lists (once per tick, capped at `kMaxCandidates`):
   - `EntityType::Monster`, valid, alive, awake, inner/outer zone
   - sorted by squared grid distance to player
   - split by `Reaction` (0 hostile, 2 friendly)
3. Evaluate rules in `Order`; for each entity in the matching list, run `CompiledExpression::Evaluate(...)`.
4. On first fire: call `onRuleFired`, set `animationLock.Engage(rule.WaitAfterPressMs)`, stop.

Candidate vectors are `thread_local` and reused each tick to avoid allocations.

---

## ScriptEngine / CompiledExpression overview

Rules are EXPRTK double expressions (non-zero = true).

### Compile stage

- Preprocess DSL (`monsterCount` / `friendlyMonsterCount` chains) → pure EXPRTK
- Intern buff/path needles; register `hasBuffIdx`, `hasBuffIdxGate`, `pathContainsIdx`, etc.
- Compute internal needs flags at compile time to skip unused host calls during evaluation

### Evaluate stage

- Bind `PluginSDK::Entity` fields to `e_*` variables
- **Cursor distance**: via `EvalTickCache::GetOrComputeCursorDistance` (shared W2S per entity per tick)
- **Buff reads**: via `EvalTickCache::GetOrLoadBuffs` using `entity.Components.Buffs`
- Per-evaluation memoization inside `CompiledExpression` for buff/path results on one entity

### Grace period

Player grace is checked once per eval tick in `EvalTickCache::PlayerHasGracePeriod()` using `Components.EnumerateBuffs()` on the player — not on every frame.

---

## Events and lifecycle

Subscriptions use the SDK pattern from ExamplePlugin:

```cpp
auto& events = const_cast<PluginSDK::EventsService&>(ctx()->Events);
events.OnAreaChange(...);
```

Unsubscribe in `OnDisable` via stored `EventsService::Token`s.

---

## Settings / UI

- `ShowGateReason` — dev toggle showing `m_LastExecutionGateReason` (persisted in `config/settings.json`)
- Test Fire uses the **selected rule's key**, not a hardcoded Q
- Host SDK incompatibility banner when `HostCompatible()` is false

---

## Common pitfalls / debugging tips

- Enable **Show execution gate (dev)** to see why rules aren't running (`In town`, `Animation wait`, etc.).
- If buff checks seem wrong, create `<pluginDir>/config/enable_buffs_dump.txt` for buff debug logging.
- Keep `.nearCursor(...)` radii tight; prefer cheap filters before buff predicates.
- After area changes, animation lock resets automatically.
