# MuffMode Licensing

[README](../README.md) | [Build Guide](build-guide.md) | [Dependency Policy](dependencies.md)

MuffMode's source license position is **GPL-2.0-only**.

The canonical repository license file is [LICENSE](../LICENSE). Core source headers use the text `Licensed under the GNU General Public License 2.0`, and the upstream Quake II Rerelease game DLL repository is published under GPL-2.0. This repo therefore treats MuffMode game-module source and project-owned modifications as GPL-2.0-only unless a file states a narrower or separate license.

This document records the repo's licensing position for engineering and release work. It is not a substitute for maintainer or legal review before public redistribution.

## Release Packages

Release packages must include:

- `LICENSE`
- `THIRD_PARTY_NOTICES.md`
- `README.html`
- `README.de.html`, `README.pl.html`, `README.fr.html`, `README.hu.html`, and `README.bg.html`
- `CHANGELOG.md`

The Windows installer displays `LICENSE` through Inno Setup. The zip package carries both `LICENSE` and `THIRD_PARTY_NOTICES.md` at the package root.

## Third-Party Code

Third-party notices are recorded in [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md). The machine-readable inventory is [docs-dev/robustness/dependency-inventory.json](../docs-dev/robustness/dependency-inventory.json).

Current third-party code dependencies:

- `{fmt}`: MIT with optional compiled-object exception.
- JsonCpp: MIT or public domain where recognized.

## Non-Code Assets

MuffMode requires a legitimate Quake II Rerelease installation. The repo must not redistribute proprietary Quake II assets outside the permissions recorded for specific files.

Community map files, original map readmes, navigation files, and package-only assets should keep their original notices in the packaging tree or in [docs/maps](maps/index.md). Do not add new non-code release assets unless their redistribution basis is recorded.

## Maintenance Rules

- Keep `LICENSE`, source headers, README badges, release package contents, and installer license display aligned.
- Update `THIRD_PARTY_NOTICES.md` and `docs-dev/robustness/dependency-inventory.json` when dependencies or vendored code change.
- Run `scripts/ci/check-dependency-inventory.ps1` before merging dependency, license, or release-packaging changes.
- Treat unreviewed dependency additions as release blockers.
