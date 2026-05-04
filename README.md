# AutoReflex — Rules (End-user)

You type a **rule expression** in the Rule editor. If it becomes **true**, AutoReflex triggers the rule’s configured key.

- **Cooldown**: how often a key can trigger (e.g. `1.0s` = at most once per second)
- **Animation wait**: small delay after a keypress to avoid interrupting the action

---

## Quick start (copy / paste)

**1) Cast when an enemy is near your cursor**

```txt
hostileMinionCount.zone(outer).nearCursor(200) > 0
```

**2) Only when the enemy has a buff**

```txt
hostileMinionCount.zone(outer).nearCursor(200).hasBuff("contagion") > 0
```

**3) Buff value / stacks example (exactly 3)**

```txt
hostileMinionCount.zone(outer).nearCursor(200).hasBuffValue("contagion", 3) > 0
```

---

## The three roots (what you start with)

- **`hostileMinionCount`**: hostile enemies only (recommended for most builds)
- **`friendlyMinionCount`**: friendly minions only
- **`corpseCount`**: dead hostile corpses only

All three roots support the same fluent filters below.

Important: the DSL is **case-sensitive**. Type the keywords exactly as shown.

---

## Fluent filters (the only ones most users need)

Add these after the root, like: `hostileMinionCount.zone(outer).nearCursor(200) > 0`

- **`.zone(inner|outer|far)`**: pick a distance bucket around your player
- **`.nearCursor(N)`**: only count enemies within `N` pixels of your cursor
- **`.type(any|normal|magic|rare|unique|atleastmagic|atleastrare|atleastunique)`**
- **`.hasBuff("buff_name")`**
- **`.hasBuffValue("buff_name", N)`**: buff charges/stacks value equals `N`
- **`.hasName("text")`**: case-insensitive substring match on monster metadata path (“skeleton”, etc.)

### Picking good `nearCursor(N)` values

- `80–150` = very near cursor
- `150–250` = near cursor
- `250+` = loose

---

## Type / rarity examples

```txt
hostileMinionCount.zone(outer).type(magic|rare).nearCursor(200) > 0
```

```txt
hostileMinionCount.zone(outer).type(atleastrare).nearCursor(200) > 0
```

---

## Advanced (optional)

You can combine checks using:

- **`and` / `or` / `not`**
- Parentheses `(` `)` for grouping

Example:

```txt
(hostileMinionCount.zone(outer).nearCursor(200).hasBuff("contagion") > 0)
and
(friendlyMinionCount.zone(inner).type(atleastrare).nearCursor(120) > 0)
```

---

## Troubleshooting

- **Expression syntax error / Translated:** the rule was converted internally and then failed to compile. The `Translated:` text shows what AutoReflex actually tried to compile.


