# AutoReflex — Rules (End-user)

You type a **rule expression** in the Rule editor. If it becomes **true**, AutoReflex triggers the rule's configured key.

- **Cooldown**: how often a key can trigger (e.g. `1.0s` = at most once per second)
- **Animation wait**: small delay after a keypress to avoid interrupting the action

---

## Quick start (copy / paste)

**1) Cast when an enemy is near your cursor**

```txt
monsterCount.nearCursor(200) > 0
```

**2) Only when the enemy has a buff**

```txt
monsterCount.nearCursor(200).hasBuff("contagion") > 0
```

**3) Buff value / stacks example (exactly 3)**

```txt
monsterCount.nearCursor(200).hasBuffValue("contagion", 3) > 0
```

---

## The one root

- **`monsterCount`** — every rule starts here.

  Defaults to **hostile** (`e_Reaction == 0`), **alive** (`e_CurrentHP > 0`), **awake** (`e_IsSleeping == 0`), within `200` px of cursor. **No implicit `e_EntityState` filter** — use raw `e_EntityState` in advanced rules if you need it (most mobs are `0` = `None` in the SDK).

Important: the DSL is **case-sensitive**. Type the keywords exactly as shown.

---

## Fluent filters (the only ones most users need)

Add these after the root, like: `monsterCount.nearCursor(150) > 0`

- **`.nearCursor(N)`**: only count enemies within `N` pixels of your cursor (overrides the default of `200`)
- **`.type(any|normal|magic|rare|unique|atleastmagic|atleastrare|atleastunique)`**
- **`.hasBuff("buff_prefix")`**
- **`.notHasBuff("buff_prefix")`**
- **`.hasBuffValue("buff_prefix", N)`**: buff charges/stacks value equals `N`
- **`.hasName("text")`**: case-insensitive substring match on monster metadata path ("skeleton", etc.)

### Picking good `nearCursor(N)` values

- `80–150` = very near cursor
- `150–250` = near cursor
- `250+` = loose

---

## Type / rarity examples

```txt
monsterCount.type(magic|rare).nearCursor(200) > 0
```

```txt
monsterCount.type(atleastrare).nearCursor(200) > 0
```

---

## Advanced (optional)

You can combine checks using:

- **`and` / `or` / `not`**
- Parentheses `(` `)` for grouping

Example:

```txt
(monsterCount.nearCursor(200).hasBuff("contagion") > 0)
and
(monsterCount.type(atleastrare).nearCursor(120) > 0)
```

If you need to bypass the default `monsterCount` filters, use raw entity fields. Available per-entity variables:

- `e_Reaction` — `0` hostile, `2` friendly
- `e_CurrentHP`, `e_MaxHP`, `e_CurrentES`, `e_MaxES`
- `e_EntityState` — SDK: `0` None, `1` Useless, `2` PlayerLeader, `3` MonsterFriendly, `4` PinnacleBossHidden (not used by implicit `monsterCount`; add manually if needed)
- `e_IsValid`, `e_IsSleeping`
- `e_Rarity` — `0` normal, `1` magic, `2` rare, `3` unique
- `e_CursorDistPx` — pixel distance from cursor to entity
- `e_GridPositionX/Y/Z`, `e_WorldX/Y/Z`

---

## Troubleshooting

- **Expression syntax error / Translated:** the rule was converted internally and then failed to compile. The `Translated:` text shows what AutoReflex actually tried to compile.
