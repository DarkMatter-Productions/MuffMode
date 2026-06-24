# MuffMode Changelog

[README](../README.md) | [Release Process](release-process.md)

This is the central change ledger for MuffMode. Every implementation should leave one grouped, user-readable row here before it is considered complete. Review the existing `Unreleased` rows first: if the new work belongs with an existing change, edit that row's summary, details, category, or magnitude instead of adding a near-duplicate row.

`Release` stays `Unreleased` until the release script stamps it with the shipped version. `Magnitude` is release-note impact: `major` changes are eligible for release highlights, `minor` changes are notable but not headline material when major changes exist, and `patch` changes are narrow fixes or maintenance.

| Release | Category | Magnitude | Summary | Details |
| --- | --- | --- | --- | --- |
| Unreleased | Gameplay and Balance | major | Q3A and Quake rulesets brought closer to their source-game feel | Expands the ruleset weapon, ammo, armor, spawn-remap, projectile, splash, knockback, and loadout behavior for Quake III Arena style and Quake style play, including Q3-style weapon mappings, shotgun and BFG tuning, Quake armor handling, and clearer player-facing ruleset documentation. |
| Unreleased | Fixes | major | Weapon think null guard prevents no-weapon crashes | Prevents a crash in `Weapon_RunThink` when accelerated weapon ticking lets a think callback switch or clear the current weapon before the next loop iteration reads `pers.weapon`. |
| Unreleased | Gameplay and Balance | minor | Horde player-scale cap raised for larger groups | Raises the default `g_horde_player_scale_max` from `4` to `8`, letting 5-8 player Horde sessions receive a proportionally larger wave point budget while preserving 1-4 player behavior and the existing per-player scale factor. |
| Unreleased | Documentation and Packaging | major | Central changelog ledger drives release notes and highlights | Replaces the temporary changelog flow with a validated central ledger, stamps unreleased rows with the release version, generates release notes from grouped entries, and limits highlights to major changes unless no major entries exist. |
| Unreleased | Documentation and Packaging | minor | Discord release intro can be manually overridden | Adds release-script support for a manual release intro through a parameter, environment variable, or local Windows text box, with Discord announcements preferring that release-note intro before falling back to generated significant-change copy. |
