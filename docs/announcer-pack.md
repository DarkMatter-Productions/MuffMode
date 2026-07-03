# MuffMode announcer voice pack

MuffMode match announcements use per-client voice lines when a player enables the announcer (`announcer on` or Player Settings → Announcer). The server requests sounds at fixed paths; clients resolve them from their local game files and pak search order.

This document lists every announcer stem, the on-disk layout, and how to ship a custom voice pack.

## Directory layout

The game DLL requests `vo_evil/<stem>.wav`, but the **client prepends `sound/`** when loading (same as `misc/tele_up.wav` → `sound/misc/tele_up.wav`). Files must live under `sound/vo_evil/`.

Place `.wav` files under `baseq2/sound/vo_evil/` or inside a `.pak` with the same paths.

```
baseq2/
├── sound/
│   └── vo_evil/
│       ├── fight.wav
│       ├── round_begins_in.wav
│       └── …
└── announcer.pak    ← optional; contains sound/vo_evil/*.wav
```

Inside a pak archive:

```
sound/vo_evil/fight.wav
sound/vo_evil/one.wav
…
```

**Canonical rules**

- **Game path:** `vo_evil/<stem>.wav` (what MuffMode requests via `soundindex`)
- **On disk / in pak:** `sound/vo_evil/<stem>.wav`

- `<stem>` is the logical name from the table below (no subfolders per event).
- File names are lowercase with underscores, matching the stem exactly.

## Audio format

Announcer files must be **uncompressed PCM `.wav`** — the same kind of audio as stock `baseq2` sounds. MuffMode only sends the path; the client loads and plays the file locally.

### Recommended

| Setting | Value |
|---------|--------|
| Container | `.wav` (RIFF WAVE) |
| Encoding | **PCM** (uncompressed) |
| Sample rate | **22050 Hz** |
| Bit depth | **16-bit** |
| Channels | **Mono** |

That is roughly **352 kbps** raw (22050 × 16 × 1). Mono is fine for voice; stereo is unnecessary.

### Also accepted

| Setting | Range / notes |
|---------|----------------|
| Sample rate | 6000–48000 Hz (`11025` or `22050` are common for short cues) |
| Bit depth | 8-, 16-, or 24-bit PCM (24-bit is truncated to 16) |
| Channels | Mono or stereo |

### Avoid

- Compressed WAV (MP3-in-WAV, ADPCM, etc.)
- Non-PCM formats
- Very long files (voice lines are usually a few seconds)

### Export examples

**Audacity:** File → Export → Export Audio → WAV → Signed 16-bit PCM, 22050 Hz, mono.

**ffmpeg:**

```bash
ffmpeg -i input.wav -ar 22050 -ac 1 -sample_fmt s16 sound/vo_evil/fight.wav
```

Batch-convert a folder (output paths preserved under `sound/vo_evil/`):

```bash
for f in *.wav; do ffmpeg -y -i "$f" -ar 22050 -ac 1 -sample_fmt s16 "sound/vo_evil/$f"; done
```

## How clients hear announcements

| Player setting | What plays |
|----------------|------------|
| Announcer **off** (default) | Stock backup sounds where defined (e.g. `misc/tele_up.wav` on match start); otherwise silence |
| Announcer **on** | `sound/vo_evil/<stem>.wav` on the client (game requests `vo_evil/<stem>.wav`) |

Vanilla Q2RE clients are unaffected unless they opt in. No server cvar selects the voice; players choose by installing paks with the correct paths.

### Recommended install: overlay pak

1. Install a **full reference pack** with every stem below.
2. Optionally add a **custom pak** later that overrides only the lines you replace.

Missing stems in a custom-only pak are **silent** for announcer-on clients (stock backups are not used on the announcer-on path today). Overlaying a full reference pack avoids gaps.

## Complete stem list

All stems below use game paths (`vo_evil/…`). On disk or in a pak, use `sound/vo_evil/…` (e.g. `sound/vo_evil/fight.wav`).

### Match and round flow

| Stem | File | Backup (announcer off) | Wired |
|------|------|------------------------|-------|
| `prepare_to_fight` | `sound/vo_evil/prepare_to_fight.wav` | — | yes |
| `prepare_your_team` | `sound/vo_evil/prepare_your_team.wav` | — | yes |
| `round_begins_in` | `sound/vo_evil/round_begins_in.wav` | — | yes |
| `fight` | `sound/vo_evil/fight.wav` | `misc/tele_up.wav` (announcer off, or layered on `CHAN_AUTO` when announcer on) | yes |
| `one` | `sound/vo_evil/one.wav` | — | yes |
| `two` | `sound/vo_evil/two.wav` | — | yes |
| `three` | `sound/vo_evil/three.wav` | — | yes |
| `round_won` | `sound/vo_evil/round_won.wav` | `ctf/flagcap.wav` | yes |
| `red_wins_round` | `sound/vo_evil/red_wins_round.wav` | `ctf/flagcap.wav` | yes |
| `blue_wins_round` | `sound/vo_evil/blue_wins_round.wav` | `ctf/flagcap.wav` | yes |

### Timers and match state

| Stem | File | Backup (announcer off) | Wired |
|------|------|------------------------|-------|
| `5_minute` | `sound/vo_evil/5_minute.wav` | — | yes |
| `1_minute` | `sound/vo_evil/1_minute.wav` | — | yes |
| `overtime` | `sound/vo_evil/overtime.wav` | `world/klaxon2.wav` | yes |
| `sudden_death` | `sound/vo_evil/sudden_death.wav` | `world/klaxon2.wav` | yes |

Countdown ticks (10, 9, … seconds) use stock `world/N.wav` / `world/Nsec.wav` backups via `MM_AnnounceRaw`, not `sound/vo_evil/` stems.

### Scoring and lead changes

| Stem | File | Backup (announcer off) | Wired |
|------|------|------------------------|-------|
| `1_frag` | `sound/vo_evil/1_frag.wav` | — | yes |
| `2_frags` | `sound/vo_evil/2_frags.wav` | — | yes |
| `3_frags` | `sound/vo_evil/3_frags.wav` | — | yes |
| `lead_taken` | `sound/vo_evil/lead_taken.wav` | — | yes |
| `lead_tied` | `sound/vo_evil/lead_tied.wav` | — | yes |
| `lead_lost` | `sound/vo_evil/lead_lost.wav` | — | yes |
| `red_leads` | `sound/vo_evil/red_leads.wav` | — | yes |
| `blue_leads` | `sound/vo_evil/blue_leads.wav` | — | yes |
| `teams_tied` | `sound/vo_evil/teams_tied.wav` | — | yes |
| `blue_scores` | `sound/vo_evil/blue_scores.wav` | — | registry only |
| `red_scores` | `sound/vo_evil/red_scores.wav` | — | registry only |

### Match end

| Stem | File | Backup (announcer off) | Wired |
|------|------|------------------------|-------|
| `you_win` | `sound/vo_evil/you_win.wav` | — | yes |
| `you_lose` | `sound/vo_evil/you_lose.wav` | — | yes |
| `red_wins` | `sound/vo_evil/red_wins.wav` | — | yes |
| `blue_wins` | `sound/vo_evil/blue_wins.wav` | — | yes |

### Combat awards

| Stem | File | Backup (announcer off) | Wired |
|------|------|------------------------|-------|
| `first_blood` | `sound/vo_evil/first_blood.wav` | — | yes |
| `rampage1` | `sound/vo_evil/rampage1.wav` | — | yes |
| `first_excellent` | `sound/vo_evil/first_excellent.wav` | — | yes |
| `excellent1` | `sound/vo_evil/excellent1.wav` | — | yes |
| `humiliation1` | `sound/vo_evil/humiliation1.wav` | — | yes |
| `double_kill` | `sound/vo_evil/double_kill.wav` | — | reserved |
| `triple_kill` | `sound/vo_evil/triple_kill.wav` | — | reserved |

### Voting

| Stem | File | Backup (announcer off) | Wired |
|------|------|------------------------|-------|
| `vote_now` | `sound/vo_evil/vote_now.wav` | `misc/pc_up.wav` | yes |
| `vote_passed` | `sound/vo_evil/vote_passed.wav` | — | yes |
| `vote_failed` | `sound/vo_evil/vote_failed.wav` | — | yes |

### Powerups (non-Q2RE rulesets)

| Stem | File | Backup (announcer off) | Wired |
|------|------|------------------------|-------|
| `quad_damage` | `sound/vo_evil/quad_damage.wav` | — | yes |
| `haste` | `sound/vo_evil/haste.wav` | — | yes |
| `battlesuit` | `sound/vo_evil/battlesuit.wav` | — | yes |
| `regeneration` | `sound/vo_evil/regeneration.wav` | — | yes |
| `invisibility` | `sound/vo_evil/invisibility.wav` | — | yes |

### CTF (reserved)

| Stem | File | Backup (announcer off) | Wired |
|------|------|------------------------|-------|
| `flag_captured` | `sound/vo_evil/flag_captured.wav` | `ctf/flagcap.wav` | reserved |

CTF flag capture still uses global stock `ctf/flagcap.wav` for all clients today. `flag_captured` is registered for a future announcer path.

## Flat file checklist (46 stems)

Copy-paste list for pack authors (paths **inside a pak** or under `baseq2/`):

```
sound/vo_evil/1_frag.wav
sound/vo_evil/1_minute.wav
sound/vo_evil/2_frags.wav
sound/vo_evil/3_frags.wav
sound/vo_evil/5_minute.wav
sound/vo_evil/battlesuit.wav
sound/vo_evil/blue_leads.wav
sound/vo_evil/blue_scores.wav
sound/vo_evil/blue_wins.wav
sound/vo_evil/blue_wins_round.wav
sound/vo_evil/double_kill.wav
sound/vo_evil/excellent1.wav
sound/vo_evil/fight.wav
sound/vo_evil/first_blood.wav
sound/vo_evil/first_excellent.wav
sound/vo_evil/flag_captured.wav
sound/vo_evil/haste.wav
sound/vo_evil/humiliation1.wav
sound/vo_evil/invisibility.wav
sound/vo_evil/lead_lost.wav
sound/vo_evil/lead_taken.wav
sound/vo_evil/lead_tied.wav
sound/vo_evil/one.wav
sound/vo_evil/overtime.wav
sound/vo_evil/prepare_to_fight.wav
sound/vo_evil/prepare_your_team.wav
sound/vo_evil/quad_damage.wav
sound/vo_evil/rampage1.wav
sound/vo_evil/red_leads.wav
sound/vo_evil/red_scores.wav
sound/vo_evil/red_wins.wav
sound/vo_evil/red_wins_round.wav
sound/vo_evil/regeneration.wav
sound/vo_evil/round_begins_in.wav
sound/vo_evil/round_won.wav
sound/vo_evil/sudden_death.wav
sound/vo_evil/teams_tied.wav
sound/vo_evil/three.wav
sound/vo_evil/triple_kill.wav
sound/vo_evil/two.wav
sound/vo_evil/vote_failed.wav
sound/vo_evil/vote_now.wav
sound/vo_evil/vote_passed.wav
sound/vo_evil/you_lose.wav
sound/vo_evil/you_win.wav
```

**Wired today:** 42 stems (everything except `blue_scores`, `red_scores`, `double_kill`, `triple_kill`, `flag_captured`).

## Related sound (not under `sound/vo_evil/`)

Powerup respawn broadcast uses a separate helper (`QLSound`), not the announcer registry:

| File | Backup (announcer off) |
|------|------------------------|
| `items/poweruprespawn.wav` | `misc/alarm.wav` |

Same announcer on/off preference applies; path is at pak root, not under `sound/vo_evil/`.

## Player command

```
announcer on   # enable vo_evil announcements
announcer off  # stock backups only (default)
```

Preference is saved in `baseq2/pcfg/sid-<id>.cfg` when the server provides a social ID.

## Source of truth

Stem names and backups are defined in [`mm_announcer.cpp`](../src/sgame/muffmode/mm_announcer.cpp) (`k_announce_defs`). When adding new events, update that table and this document together.
