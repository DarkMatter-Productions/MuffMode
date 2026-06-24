<div align="center">
  <img src="assets/img/logo.png" alt="Muff Mode logo" width="600">

  [![Latest Release](https://img.shields.io/github/v/release/DarkMatter-Productions/MuffMode?label=release&color=blue)](https://github.com/DarkMatter-Productions/MuffMode/releases/latest)
  [![License](https://img.shields.io/github/license/DarkMatter-Productions/MuffMode?color=blue)](LICENSE)
  [![Build](https://img.shields.io/github/actions/workflow/status/DarkMatter-Productions/MuffMode/build.yml?label=build)](https://github.com/DarkMatter-Productions/MuffMode/actions/workflows/build.yml)
  [![Open Issues](https://img.shields.io/github/issues/DarkMatter-Productions/MuffMode)](https://github.com/DarkMatter-Productions/MuffMode/issues)
  [![Stars](https://img.shields.io/github/stars/DarkMatter-Productions/MuffMode?style=flat)](https://github.com/DarkMatter-Productions/MuffMode/stargazers)
  
  <h1>Muff Mode</h1>
  <p><strong>Server-side multiplayer upgrades for Quake II Remastered.</strong></p>
  <p>
    <a href="https://discord.gg/T32mFejwR4">
      <img src="https://img.shields.io/badge/Join%20us%20on-Discord-5865F2?style=for-the-badge&logo=discord&logoColor=white" alt="Join us on Discord">
    </a>
  </p>
</div>

**Muff Mode** is a **server-side multiplayer mod** for [Quake II Rerelease](https://bethesda.net/en/game/quakeii), built to make every kind of session feel better: **drop-in public games**, **serious competitive matches**, and **well-run community servers**. It combines a more readable match experience for players with deeper match control for hosts, so the same install can support a quick casual night, a pickup, or a polished event.

<h2 align="center">Who It's For</h2>

| Audience | What Muff Mode gives you |
| --- | --- |
| Casual players | **Clearer HUD information**, **menu-driven voting**, approachable team joining, and extra modes such as **Horde**, **Instagib**, and **NadeFest**. |
| Competitive players | **Ready-up flow**, **countdowns**, **timeouts**, **overtime**, support for **Duel / TDM / CA / CTF**, flexible rulesets, and captain-led team control. |
| Server hosts | **Per-gametype configs**, curated map rotations, voting limits, admin tools, team management, **debug logging**, and built-in diagnostics like `doctor`. |

<h2 align="center">Start Here</h2>

| I want to... | Read |
| --- | --- |
| Install, join games, vote, bind the hook, or learn player commands | [Player Guide](docs/player-guide.md) |
| Run a public server, private lobby, pickup, or scrim server | [Server Host Guide](docs/server-host-guide.md) |
| Compare gametypes, game modifications, and maps | [Gameplay Reference](docs/gameplay-reference.md) |
| Choose a ruleset or compare weapon, item, ammo, and movement feel | [Rulesets](docs/rulesets.md) |
| Browse included `mm-*` map remasters, ports, original readmes/BSPs, source maps, item registers, and history | [Muff Mode Map Guide](docs/maps/index.md) |
| Look up commands, cvars, voting flags, and config files | [Configuration Reference](docs/configuration-reference.md) |
| Build maps or entity overrides for Muff Mode | [Level Design Guide](docs/level-design-guide.md) |
| Build or publish the Windows updater | [Updater Guide](docs/updater-guide.md) |
| Compile the DLL from source | [Build Guide](docs/build-guide.md) |
| Reproduce hardening, test, analysis, fuzz, and release gates | [Hardening Guide](docs/hardening-guide.md) |
| Review license and dependency notices | [Licensing](docs/licensing.md) and [Dependency Policy](docs/dependencies.md) |
| Track unreleased changes and release highlights | [Changelog](docs/changelog.md) |
| Prepare and publish a release package | [Release Process](docs/release-process.md) |

<h2 align="center">Quick Install</h2>

1. Download the [latest Muff Mode release](https://github.com/DarkMatter-Productions/MuffMode/releases/latest).
2. Use the **Windows installer** when available. It detects **Steam**, **Epic Games Store**, and **GOG** installs, and also offers an **Other location** option for custom library folders.
3. If you use the zip instead, extract it into the outer **`Quake 2`** folder and allow file replacements.
4. Launch the game normally. Server hosts can execute the bundled server config with **`exec muff-sv.cfg`** when it is included in the release package.

For a more careful walkthrough, use the [Player Guide](docs/player-guide.md) or [Server Host Guide](docs/server-host-guide.md).

<h2 align="center">Highlights</h2>

 - **Built for every lobby style** — supports **casual public servers**, **private friend sessions**, **pickups**, **scrims**, and more organized competitive events.
 - **A fuller mode lineup** — includes **Deathmatch**, **Duel**, **TDM**, **CTF**, **Clan Arena**, **CaptureStrike**, **Red Rover**, **Last Man Standing**, **Horde**, **Freeze Tag**, **ProBall**, **Instagib**, and **NadeFest**.
 - **Flexible rulesets** — switch between [**Quake II Rerelease**, **Muff Mode**, **Q2RE Balanced**, **Quake III Arena style**, **Quake style**, and **Quake Champions style**](docs/rulesets.md) depending on the feel you want.
 - **Better match flow** — warmups, ready checks, countdowns, post-match delays, sudden death, overtime, round handling, and timeouts help games start cleanly and stay organized.
 - **Player-friendly interface improvements** — a purpose-built HUD, compact scoreboard, frag messages, timer support, help text, MOTD access, and match-state feedback keep important information visible without overwhelming the screen.
 - **Fast, accessible voting** — use **GUI or console voting** for maps, gametypes, rulesets, server settings, and administrative actions, with host-side controls to keep votes focused.
 - **Stronger teamplay tools** — captains, captain transfer, team locking, auto-balance, forced balance, and team item-drop notices make coordinated play easier to run.
 - **Quality-of-life touches players notice** — **kill beeps**, **offhand hook support**, **EyeCam spectating**, **MyMap queueing**, and smarter auto-switch behavior all help matches feel smoother.
 - **Host controls that scale up** — tune per-gametype configs, map lists and pools, votable options, MOTDs, debug logging, and server diagnostics without needing separate builds for different communities.
 - **Modding and map support** — Muff Mode also adds **custom maps**, **entity overrides**, **item replacement**, conditional entity spawning, and new mapper-focused entities for more tailored server content.

<h2 align="center">Included Content</h2>

**Muff Mode** releases are intended to include the **game logic DLL**, **server configuration material**, **bot support files**, **map entity overrides**, and the **Windows updater**. The source repository contains the C++ game code, Visual Studio project files, updater source, documentation, packaging assets, and project media used to ship the mod.

<h2 align="center">Development</h2>

The project builds on **Windows** with **Visual Studio 2022 / MSBuild**. See the [Build Guide](docs/build-guide.md) for prerequisites, commands, output location, and local test installation.

---

<h2 align="center">Credits</h2>

This project was started by [themuffinator](https://github.com/themuffinator), later with regular and significant development/maintenance/testing by [ozy](https://github.com/ozy24). It would not be possible without the outstanding work from the Nightdive Team who worked on [Quake II Rerelease](https://bethesda.net/en/game/quakeii) ([source code here](https://github.com/id-Software/quake2-rerelease-dll)).
Muff Mode exists thanks to the [DarkMatter Discord community](https://discord.gg/T32mFejwR4), the Nightdive team, id Software, the Quake II Rerelease player community, [Paril's Q2 Horde work](https://github.com/Paril/q2horde), ceeeKay's EyeCam code from [Q2Eaks](https://github.com/ceeeKay/Q2Eaks), and the [Stingy Hat Games modding tutorial](https://www.youtube.com/watch?v=PiSMiS3Epyk&t=261s&pp=ygUYU3Rpbmd5IEhhdCBHYW1lcyBxdWFrZSAy0gcJCTkLAYcqIYzv).

<h2 align="center">Disclaimer</h2>

**Muff Mode** is an independent project and is not affiliated with, endorsed by, or sponsored by Nightdive Studios, id Software, Bethesda, or ZeniMax Media. Quake II is a trademark of ZeniMax Media Inc.

The software is provided "as is" without warranty of any kind. **Muff Mode** requires a legitimate Quake II Rerelease installation.

---

See [LICENSE](LICENSE), [Licensing](docs/licensing.md), and [Third-Party Notices](THIRD_PARTY_NOTICES.md) for license details.
