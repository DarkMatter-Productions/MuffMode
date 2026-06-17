# MuffMode Crash Policy

Date: 2026-06-15
Branch: `muffdev`

This document records when hard failures are appropriate and when MuffMode should reject input or warn and continue. The goal is to keep `Com_Error` reserved for corrupted internal state and unsafe load states, not ordinary player/admin input mistakes.

## Categories

| Category | Use for | Required behaviour |
|---|---|---|
| Fatal | Broken engine/game contract, corrupted save state that cannot be recovered safely, unsupported persisted type metadata, invalid shared-structure assumptions, or impossible internal invariants. | Call `Com_Error` / `Com_ErrorFmt` with enough context to diagnose the failure. |
| Reject input | Malformed user, admin, vote, config or command arguments where the current game state can continue unchanged. | Return without applying partial state and print a client/admin-visible error when a client exists. |
| Warn and continue | Optional data, map quirks, cosmetic config, or non-strict save fields that can be skipped without invalidating core state. | Print a warning and keep the fallback explicit. |

## Command Parsing

Client/admin command parsing is reject-input by default. Bad numbers, missing arguments, odd key/value lists and out-of-range values must not silently coerce to zero, and must not call `Com_Error`.

Examples on `muffdev`:

- `teleport` accepts exactly `teleport <x> <y> <z>` or `teleport <x> <y> <z> <pitch> <yaw> <roll>`.
- `spawn` requires a classname and an even number of key/value tail arguments.
- `use_index`, `drop_index`, `wave`, `ghost`, `killbeep`, `give health`, and ammo counts reject malformed integers.

## Save And Load

Save/load code is fatal when continuing could produce invalid entities, invalid pointer links, or incompatible persisted type metadata. It may warn and continue only when the existing strict-save policy allows the field to be skipped safely.

Rules:

- `g_strict_saves` upgrades recoverable JSON field warnings to fatal errors.
- Root-shape errors, malformed top-level arrays/objects, unsupported persisted type IDs, and invalid persisted item/data pointers are fatal.
- Missing optional fields or non-critical malformed fields may warn only when the reader supplies a safe default and the save remains internally consistent.

## Formatting And Localisation

Formatted print/error wrappers use per-call scratch storage. Localised print wrappers also embed numeric arguments into per-call storage and pass them to `Loc_Print` synchronously. Do not store pointers to these temporary localisation buffers beyond the `Loc_Print` call.

## Review Rule

New parser or load sites must choose one category in review. If a site calls `Com_Error`, the review should be able to answer: "what invariant would be unsafe to continue without?"
