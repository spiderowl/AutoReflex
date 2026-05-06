# AutoReflex Internals (Design + Intent)

This document explains **how AutoReflex works internally**, and **why** the key functions exist.
It is written for future maintenance (performance + correctness) and aims to match the current code.

Constraints / invariants we intentionally keep:

- **Never modify `sdk/` or `imgui/`** in this repo (keep parity with upstream POEFixer/ExamplePlugin).
- **Do not change `e_Zone` logic** (if/where it exists elsewhere); performance work should not “paper over” upstream semantics.
- Hot path goals: **cheap filters first**, avoid host calls unless required, avoid repeated work across many rules.

---

## High-level pipeline

AutoReflex runs as a POEFixer plugin. The host calls `AutoReflexPlugin::DrawUI()` every frame.

The plugin does **one snapshot fetch per tick** and then (throttled) evaluates rules:

1. `AutoReflexPlugin::DrawUI()`:
   - checks host attach state
   - calls `ctx->GetSnapshot()` once
   - calls `ShouldExecute(...)` once (cheap “global gate”)
   - if throttled interval elapsed, calls `RuleManager::EvaluateAll(...)`
2. `RuleManager::EvaluateAll(...)`:
   - builds two candidate lists once per tick:
     - hostile monsters (`monsterCount` root)
     - friendly monsters (`friendlyMonsterCount` root)
   - evaluates rules **in `Order`** until the first rule fires
3. `CompiledExpression::Evaluate(...)`:
   - binds entity fields to EXPRTK variables (`e_*`)
   - only performs expensive host calls if the expression needs them:
     - `WorldToScreen` when cursor-distance is referenced / required by gating
     - `ReadBuffsComponent` only when a buff predicate is reached
4. If a rule fired, AutoReflex presses the configured key.

Important behavior decisions:

- **One fire per evaluation tick**: once any rule triggers, remaining rules are skipped for that tick.
- Evaluation is throttled (target cadence configured by plugin settings; often ~33ms).

---

## Key files and responsibilities

- `AutoReflex.cpp`
  - plugin lifecycle (`SetContext`, `OnEnable`, `DrawUI`)
  - evaluation throttling and key press dispatch
  - config-based enabling of buffs dump logging

- `rules/RuleManager.cpp`
  - loads rules, compiles them, and evaluates them
  - builds the per-tick candidate entity lists (hostile vs friendly)
  - stops after the first fired rule each tick (intentional)

- `rules/Rule.h`
  - `Rule` definition (user-editable + runtime fields)
  - `RuleRoot` cache to avoid string-searching the script body every tick

- `scripting/ScriptEngine.{h,cpp}`
  - `CompiledExpression`: compile + evaluate expressions
  - DSL preprocessing: translates `monsterCount.<chain> > N` into EXPRTK-ready expressions
  - performance gating (needs flags, aim-gated buff reads, per-evaluation caches)
  - optional debug dump logging of buff names/values returned by the host

- `README.md`
  - end-user rule syntax (not engine internals)

---

## Rule structure and lifecycle

### `Rule` (what it means)

From `rules/Rule.h`:

- **User fields (persisted)**:
  - `Name`: display name
  - `Enabled`: toggle evaluation
  - `Key`: Windows virtual key code to press when true
  - `CooldownSec`: minimum time between fires
  - `WaitAfterPressMs`: animation safety delay (implementation may live in key sender)
  - `Order`: evaluation ordering (lower runs earlier)
  - `ScriptBody`: expression string typed by the user

- **Runtime fields (not persisted)**:
  - `CompiledExpr`: compiled EXPRTK expression + translation caches
  - `CompileError`: error message shown in UI
  - `LastEvalResult`: last tick’s boolean result (UI feedback)
  - `LastFired`, `EverFired`: runtime stats
  - `Root`: cached `RuleRoot`:
    - `Hostile` for `monsterCount`
    - `Friendly` for `friendlyMonsterCount`

### `RuleManager::CompileRule(Rule&)`

Intent:

- Validate/compile once at load/edit time, not in the hot loop.
- Precompute `rule.Root` by checking whether `ScriptBody` contains `friendlyMonsterCount`.
  - This avoids doing `find(...)` per tick.

### `RuleManager::EvaluateAll(ctx, snapshot, onFire)`

Intent:

- Minimize total work for many rules by doing shared work once per tick.
- Build a *small* list of candidates and iterate that list for each rule (not the full snapshot).
- Evaluate rules in the user-defined order and stop once a rule fires.

Hot-path structure:

1. Build candidate lists (once):
   - Iterate `snapshot->Entities` once.
   - Keep only:
     - `entityType == Monster`
     - `IsValid`
     - `CurrentHP > 0` (alive)
     - `IsSleeping == false` (awake)
     - zone is Inner or Outer (cheap scan shrink)
   - Score by squared **grid** distance to player (cheap: no `sqrt`).
   - Keep the **closest N** (cap is `kMaxCandidates`, currently 100).
   - Split into hostile (`Reaction==0`) and friendly (`Reaction==2`).

2. Evaluate rules:
   - Choose scan set based on `rule.Root`:
     - hostileMonsters for `monsterCount`
     - friendlyMonsters for `friendlyMonsterCount`
   - For each entity pointer in the scan set, run `rule.CompiledExpr->Evaluate(ctx, *entity)`.
   - If any entity makes the expression true, the rule fires.

3. One-fire-per-tick behavior:
   - After `onFire(rule)` is called once, we skip the rest of the rules in that tick.
   - `LastEvalResult` for skipped rules is set to false for that tick (UI clarity).

Vector reuse optimization:

- The candidate vectors are `thread_local` and cleared each tick so we **do not allocate** at 30Hz.
- `reserve(...)` is done once to avoid capacity growth re-allocations.

---

## ScriptEngine / CompiledExpression overview

AutoReflex rules are evaluated by EXPRTK (`exprtk.hpp`) as **double-valued expressions**.
We treat non-zero as true.

### Compile stage: `CompiledExpression::Compile(exprString, errorMsg)`

Intent:

- Convert human-friendly DSL to a pure EXPRTK expression string.
- Intern strings (buff names, path substrings) into arrays so runtime checks use integer indices.
- Register functions used by the translated expression:
  - `hasBuffIdx(idx)`
  - `hasBuffValueIdx(idx)`
  - `hasBuffIdxGate(idx, limitSq)`
  - `hasBuffValueIdxGate(idx, limitSq)`
  - `pathContainsIdx(idx)`

Key outputs:

- `compiledString_`: final EXPRTK text compiled by `parser_->compile(...)`
- `buffNeedles_`: list of unique buff names referenced in the rule (interned)
- `pathNeedles_` / `pathNeedlesLower_`: path substring needles for `.hasName("...")`
- “needs flags” computed from the compiled string:
  - `needsCursorPx_`: expression references `e_CursorDistPx`
  - `needsCursorSq_`: expression references `e_CursorDistSq`
  - `needsBuffs_`: expression uses any buff function
  - `needsPath_`: expression uses `pathContainsIdx(...)`
  - `needsCursorForBuffGate_`: expression uses any `...Gate(...)` function

### Evaluate stage: `CompiledExpression::Evaluate(ctx, entity) -> bool`

Intent:

- Bind `RadarEntity` fields to `e_*` variables without copying.
- Only call expensive host bridge APIs when required.

Per-evaluation binding:

- Always bind cheap scalar fields from `RadarEntity`:
  - `e_Id`, `e_IsValid`, `e_Rarity`, `e_EntityState`, `e_CurrentHP`, `e_MaxHP`, …
  - `e_Reaction` is copied from `entity.Reaction` (0 hostile, 2 friendly)

Host calls:

- **Cursor distance** (`WorldToScreen`):
  - Only computed if `needsCursorPx_` or `needsCursorSq_` or `needsCursorForBuffGate_`.
  - `e_CursorDistSq` is computed as \(dx^2 + dy^2\).
  - `e_CursorDistPx` (`sqrt`) is computed **only** when needed.

- **Buff reads** (`ReadBuffsComponent`):
  - Never called unless a buff predicate is evaluated.
  - Within a single `Evaluate(...)` call, we cache buff data so multiple buff checks don’t re-read.

Result:

- `expression_->value()` returns double; non-zero means the rule matched for this entity.

---

## monsterCount / friendlyMonsterCount DSL translation

Users write expressions like:

```txt
monsterCount.nearCursor(200).type(atleastmagic).notHasBuff("curse_chaos_weakness") > 0
```

`PreprocessExpression(...)` finds these chains and rewrites them into EXPRTK expressions that count matches.

### Shared translation core: `TranslateMonsterCountChainImpl(root, defaultReaction, ...)`

Intent:

- Convert fluent chains into a boolean predicate, with ordering chosen for performance.

Internal condition buckets:

- **coreConds** (cheap, no host calls):
  - reaction match (hostile/friendly)
  - alive (`e_CurrentHP > 0`)
  - awake (`e_IsSleeping == 0`)
  - rarity/type filters
  - other cheap filters

- **aimConds** (requires cursor projection / `WorldToScreen`):
  - `.nearCursor(N)` becomes `e_CursorDistSq <= N*N`

- **buffConds** (requires `ReadBuffsComponent`):
  - `.hasBuff("x")` / `.notHasBuff("x")`
  - `.hasBuffValue("x", n)`
  - default exclusion of `hidden_monster` (added as a buff condition)

Key design rule:

- We evaluate **core → aim → buffs** so that expensive work is only done for entities that pass cheap checks.

### Aim-gated buff reads: `hasBuffIdxGate` / `hasBuffValueIdxGate`

Problem:

- Even with “core → aim → buff” ordering, the expression evaluator might still reach buff functions in ways that cause extra work unless we make the gating explicit.

Solution:

- Translate buff conditions to `hasBuff*Gate(idx, aimLimitSq)` when the rule has/uses `.nearCursor(...)` (explicit or implicit default).
- `HasBuffIdxGate(idx, limitSq)` does:
  - if `e_CursorDistSq > limitSq` return false immediately
  - else call `HasBuffIdx(idx)` (which can read buffs)

So: **entities outside the cursor radius never trigger a buff read**, even if the expression structure changes.

### Root variants

- `monsterCount`: compiled with `defaultReaction = 0` (hostile)
- `friendlyMonsterCount`: compiled with `defaultReaction = 2` (friendly)

Both share the same fluent methods and translation logic; only the default reaction differs.

---

## Buff address resolution and debug dump logging

### Why we need fallback address resolution

In some snapshots, `RadarEntity.ComponentCache.BuffsAddr` may be 0 even when buffs exist.
We do **not** modify the SDK, so we can’t “fix” the struct; instead we implement a plugin-side fallback:

- `ResolveBuffsAddr(ctx, ent, outUsedFallback)`
  - uses `ent.ComponentCache.BuffsAddr` if present
  - otherwise scans `ctx->GetEntityDebugList()` and picks a component address whose name contains “buff”

Performance guard:

- `TryGetBuffsAddrFromDebugList(...)` caches the debug list mapping and refreshes at most every 500ms.

### Buffs dump file (user debugging)

Goal:

- Capture the exact buff names returned by the host for diagnosis (e.g., `hidden_monster`).

Controls:

- Compile-time: enabled only in `_DEBUG` or when `AUTOREFLEX_ENABLE_BUFFS_DUMP` is defined.
- Runtime: enabled only when `ScriptEngine::SetBuffsDumpEnabled(true)` is set.
  - AutoReflex enables this if `<pluginDir>/config/enable_buffs_dump.txt` exists.

Output:

- Appends to `<pluginDir>/config/AutoReflex_BuffsDump.txt` (path set from `AutoReflex.cpp`).
- Logs each entity once per run (to limit size), but ALWAYS logs lines tagged `Evaluate_TRUE...`.

Function: `AppendBuffsDebugLine(ent, dataOrNull, tag)`

- **`ent`**: the entity being evaluated
- **`dataOrNull`**: either null (no data) or the returned `PluginBuffsData`
- **`tag`**: a short label like:
  - `Evaluate`, `Evaluate_FallbackAddr`
  - `Evaluate_TRUE`, `Evaluate_TRUE_FallbackAddr`
  - `ReadBuffsComponent`, `ReadBuffsComponent_FallbackAddr`
  - `ReadBuffsComponent_NoAddr`

---

## Per-evaluation caches (inside one Evaluate call)

These caches exist to avoid repeated work *within* one expression evaluation on one entity:

- `buffsCacheData_` via `GetBuffsDataCached(...)`:
  - ensures at most one `ReadBuffsComponent(...)` call per entity evaluation
- `buffResultCache_` / `buffValueCache_`:
  - memoize `hasBuffIdx(idx)` and `hasBuffValueIdx(idx)` results by needle index
- `pathLowerScratch_` + `pathLowerReady_`:
  - lowercases the entity path only once when `.hasName(...)` is used

We intentionally did **not** add a shared “per tick / across rules” buff cache because:

- typical usage here is ~10 rules with only 2–3 buff rules, and
- one-fire-per-tick reduces the amount of work after the first match anyway.

---

## API / function reference (intent + parameters)

### `AutoReflexPlugin::SetPluginDirectory(const char* dir)`

- **Intent**: store plugin directory and configure optional debug dump output path.
- **Important behavior**:
  - sets dump path to `<dir>/config/AutoReflex_BuffsDump.txt`
  - enables dump only if `<dir>/config/enable_buffs_dump.txt` exists

### `AutoReflexPlugin::DrawUI()`

- **Intent**: main “tick” without drawing UI; drives evaluation on a throttled cadence.
- **Parameters**: none (host callback).
- **Key invariant**: exactly one snapshot per call; avoids multiple `GetSnapshot()` calls.

### `RuleManager::EvaluateAll(PluginContext* ctx, const PluginGameSnapshot* snapshot, onFire)`

- **Intent**: shared candidate scan + rule evaluation.
- **`ctx`**: host bridge; provides `WorldToScreen`, `ReadBuffsComponent`, debug list, etc.
- **`snapshot`**: immutable view of current game state; provides entities + player.
- **`onFire(rule)`**: callback invoked for the first rule that fires in this tick (keypress is handled outside).

### `CompiledExpression::Compile(const std::string& exprString, std::string& errorMsg)`

- **Intent**: preprocess DSL → compile with EXPRTK → compute “needs” flags.
- **`exprString`**: raw user script body.
- **`errorMsg`**: on failure, contains a user-facing explanation (often includes translated expression).

### `CompiledExpression::Evaluate(PluginContext* ctx, const RadarEntity& entity) const`

- **Intent**: evaluate the compiled rule against a single entity.
- **`ctx`**: host bridge (may be null checked).
- **`entity`**: entity to bind into `e_*` variables.
- **Returns**: `true` if expression evaluates non-zero.

### `CompiledExpression::HasBuffIdx(int idx) const`

- **Intent**: return whether the current entity has buff needle `buffNeedles_[idx]`.
- **`idx`**: index into interned buff needles.
- **Notes**: uses `GetBuffsDataCached(...)` to avoid repeated reads.

### `CompiledExpression::HasBuffIdxGate(int idx, double limitSq) const`

- **Intent**: cheap spatial gate for buff checks.
- **`limitSq`**: squared pixel radius (same coordinate space as `e_CursorDistSq`).
- **Behavior**: returns false without reading buffs if the entity is outside the aim radius.

### `CompiledExpression::HasBuffValueIdx(int idx) const`

- **Intent**: read a numeric buff “value” (currently charges/stacks) for the named buff.
- **Returns**: `0` if absent or unavailable.

### `CompiledExpression::HasBuffValueIdxGate(int idx, double limitSq) const`

- Same as `HasBuffIdxGate`, but returns 0 when outside the gate.

### `CompiledExpression::PathContainsIdx(int idx) const`

- **Intent**: case-insensitive substring match on the entity path.
- **`idx`**: index into lowercased needles from `.hasName("...")`.

---

## Common pitfalls / debugging tips

- If AutoReflex seems to “cast on hidden monsters”:
  - enable buffs dump file and verify the entity actually had `hidden_monster` in the returned buff list
  - verify your rule uses `monsterCount` (hostile) vs `friendlyMonsterCount`
  - confirm `.nearCursor(...)` radius is reasonable (very large radii may include unwanted entities)

- If `ComponentCache.BuffsAddr` logs as 0:
  - this can be normal; we use a fallback mapping from the debug list when needed.

- If performance drops with many rules:
  - keep `.nearCursor(...)` tight
  - prefer cheap conditions first
  - avoid buff predicates unless necessary

