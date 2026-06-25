# MuffMode Dependency Policy

[README](../README.md) | [Build Guide](build-guide.md) | [Licensing](licensing.md)

MuffMode uses a **documented dual-path dependency model** for the C++ game DLL:

- `vcpkg.json` at the repository root is the authoritative dependency manifest and baseline for reproducible builds.
- `third_party/fmt` and `third_party/jsoncpp` are vendored compatibility mirrors kept outside `src/` so the source tree contains project code, not dependency code.
- Vendored copies must match the versions recorded in [docs-dev/robustness/dependency-inventory.json](../docs-dev/robustness/dependency-inventory.json).

Do not add a new dependency, update `vcpkg.json`, or replace vendored dependency code without updating:

- [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md)
- [docs-dev/robustness/dependency-inventory.json](../docs-dev/robustness/dependency-inventory.json)
- [docs/licensing.md](licensing.md), if the license position changes
- Release packaging checks, if notices or source obligations change

## Current Dependencies

| Dependency | Manifest source | Vendored source | Version | License |
|---|---|---|---:|---|
| `{fmt}` | `vcpkg.json` dependency `fmt` | `third_party/fmt` | 10.1.1 | MIT with optional compiled-object exception |
| JsonCpp | `vcpkg.json` dependency `jsoncpp` | `third_party/jsoncpp` | 1.9.5 | MIT or public domain where recognized |

The DLL currently builds `{fmt}` header-only through `FMT_HEADER_ONLY`. JsonCpp headers are available locally under `third_party/jsoncpp/include/json`, while the library is linked from the vcpkg static package.

## Enforcement

Run:

```powershell
./scripts/ci/check-dependency-inventory.ps1
```

The script verifies:

- Root `LICENSE` is GPL-2.0.
- `vcpkg.json` still declares `fmt` and `jsoncpp` at the recorded baseline.
- Vendored `{fmt}` and JsonCpp header versions match the inventory.
- `THIRD_PARTY_NOTICES.md` and release packaging include third-party notices.

The build workflow runs the same check after vcpkg setup.

## Updater

The Windows updater currently uses only .NET SDK/framework libraries and has no explicit NuGet package dependencies. If NuGet packages are added, they must be added to the dependency inventory and notices before release.
