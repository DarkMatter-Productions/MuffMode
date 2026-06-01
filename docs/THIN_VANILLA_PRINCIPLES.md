# "Thin Vanilla" Design Principles for MuffMode

## Philosophy
MuffMode follows a **thin vanilla** architecture:
- Keep changes to vanilla Quake II code minimal and intentional.
- Place new feature logic in `muffmode/` (or another dedicated mod folder).
- Maintain a clean boundary between upstream code and MuffMode code.
- Make MuffMode features easy to find, test, disable, or remove.

## Why This Matters

### 1. Maintainability
- Upstream vanilla updates merge with fewer conflicts.
- MuffMode features stay self-contained and easier to reason about.
- Ownership is clearer: "core vanilla" vs "mod behavior."

### 2. Debuggability
- Regressions are easier to isolate (vanilla hook vs MuffMode logic).
- Features can be toggled for verification and rollback.
- Change origin is explicit through standardized markers.

### 3. Upstream Respect
- Avoids spreading project-specific logic through baseline files.
- Keeps vanilla files readable for contributors and reviewers.
- Reduces long-term maintenance friction.

### 4. Modularity
- Features can be enabled/disabled with cvars or config gates.
- Independent features can be removed without broad refactors.
- Functionality is more portable to future branches/mods.

## Implementation Guidelines

### ✅ DO: Keep Vanilla Hooks Small

**Good Example** - add a single toggle check:
```cpp
// In g_main.cpp (vanilla file)
// [MuffMode] Map rotation control toggle
cvar_t *mm_map_shuffle_once;

// [MuffMode] Skip re-shuffle when one-time shuffle is active
if (g_map_list_shuffle->integer && !mm_map_shuffle_once->integer)
{
    // Existing vanilla shuffle code
}
```

**Why this is good:**
- Adds only a declaration and a focused conditional hook.
- Preserves vanilla signatures and control flow.
- Uses clear `[MuffMode]` markers for fast auditing.

### ✅ DO: Put Behavior in MuffMode Modules

**Good Example** - implementation in mod namespace/folder:
```cpp
// In muffmode/mm_main.cpp (MuffMode file)
namespace {
    std::vector<std::string> mm_split(std::string_view input, char delim) {
        // Local helper logic
    }
}

void MM_ShuffleMapList() {
    auto values = mm_split(g_map_list->string, ' ');
    std::shuffle(values.begin(), values.end(), mt_rand);
    // ... rest of MuffMode behavior
}
```

**Why this is good:**
- Complex logic stays out of vanilla files.
- Helpers remain local to the feature module.
- Reuses engine/vanilla infrastructure with minimal coupling.

### ✅ DO: Reuse Existing Infrastructure

**Good Example** - avoid duplicate utilities:
```cpp
extern std::mt19937 mt_rand;          // Existing RNG
gi.cvar_set("g_map_list", "...");     // Existing cvar system
join_strings(values, " ");            // Existing utility helper
```

**Why this is good:**
- Reduces duplicated logic and drift risk.
- Keeps behavior consistent with existing systems.
- Minimizes code surface area.

### ❌ DON'T: Move Feature Bodies into Vanilla Files

**Bad Example:**
```cpp
// In g_main.cpp (vanilla file)
void ShuffleMapListOnce() {
    // 50 lines of MuffMode-specific behavior
    // Should live in muffmode/
}
```

**Why this is bad:**
- Blurs boundaries between vanilla and MuffMode.
- Increases merge conflicts and review cost.
- Makes rollback/removal harder.

### ❌ DON'T: Change Vanilla Signatures for Mod Needs

**Bad Example:**
```cpp
// In g_main.cpp
void EndDMLevel(bool muffmode_skip_shuffle)  // BAD
{
    // ...
}
```

**Why this is bad:**
- Creates broad API churn for a local feature.
- Forces unrelated call-site changes.
- Complicates future upstream sync.

### ❌ DON'T: Reimplement Existing Helpers

**Bad Example:**
```cpp
// In muffmode/mm_main.cpp
namespace {
    std::string join_strings(...) { }  // BAD: duplicate helper
}
```

**Why this is bad:**
- Introduces duplicate behavior paths.
- Increases inconsistency risk over time.
- Expands maintenance overhead.

## Code Organization

### Preferred Layout
```text
src/
|-- g_*.cpp              # Vanilla files (minimal MuffMode hooks)
|-- p_*.cpp              # Vanilla files (minimal MuffMode hooks)
|-- m_*.cpp              # Vanilla files (minimal MuffMode hooks)
`-- muffmode/            # MuffMode-specific code
    |-- muffmode.h       # Public MuffMode API
    |-- mm_main.cpp      # Core systems
    |-- mm_hud.cpp       # HUD features
    |-- mm_menu.cpp      # Menu/UI features
    `-- mm_*.cpp         # Other MuffMode modules
```

### Change Marking Standard
Always annotate vanilla edits:
```cpp
// [MuffMode] Brief why + what
code_here();

// [MuffMode] Feature gate
if (mm_feature_enabled->integer) {
    MM_DoSomething();
}
```

## Practical Example: `mm_map_shuffle_once`

### Vanilla-side touch points
1. Cvar declaration/init
2. One conditional hook before existing shuffle behavior
3. One call into MuffMode implementation

**Target footprint:** single-digit line changes in vanilla files.

### MuffMode-side implementation
1. Local helper(s), if needed
2. Main feature function in `muffmode/`
3. Optional debug logging behind a cvar gate

**Target footprint:** the bulk of logic lives in MuffMode files.

### Outcome
- Vanilla stays clean and reviewable.
- MuffMode behavior stays modular and testable.
- Feature rollback is low-risk.

## Checklist for New MuffMode Features

Before merging a feature:
- [ ] Is most logic implemented in `muffmode/`?
- [ ] Are vanilla edits minimal and clearly marked `[MuffMode]`?
- [ ] Are vanilla changes hooks/config gates, not feature bodies?
- [ ] Are existing utilities reused instead of duplicated?
- [ ] Can the feature be disabled cleanly via cvar/config?
- [ ] Will upstream vanilla sync remain straightforward?
- [ ] Is behavior documented for future maintainers?

## Anti-Patterns to Avoid

### The "Swiss Army Vanilla"
Packing feature logic into vanilla files because it feels faster short-term.

### The "Duplicate Helper"
Rewriting utilities that already exist in shared/vanilla code.

### The "Silent Invader"
Editing vanilla files without explicit `[MuffMode]` annotations.

### The "Signature Breaker"
Changing vanilla function signatures for MuffMode-specific flags/data.

### The "Mandatory Feature"
Shipping behavior with no clean off-switch or rollback path.

## Decision Rule

When unsure, ask:
**"Can this stay in `muffmode/` with only a tiny vanilla hook?"**

If the answer is "yes," do that.

