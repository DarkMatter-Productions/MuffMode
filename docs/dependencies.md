# MuffMode Dependency Policy

[README](../README.md) | [Build Guide](build-guide.md) | [Licensing](licensing.md)

MuffMode uses a **documented dual-path dependency model** for the C++ game DLL:

- `src/vcpkg.json` is the authoritative dependency manifest and baseline for reproducible builds.
- `src/fmt`, `src/json`, `src/fmt.cc`, `src/format.cc`, and `src/os.cc` are vendored compatibility mirrors kept in the source tree because the current project include path resolves those headers through `$(ProjectDir)`.
- Vendored copies must match the versions recorded in [docs-dev/robustness/dependency-inventory.json](../docs-dev/robustness/dependency-inventory.json).

Do not add a new dependency, update `src/vcpkg.json`, or replace vendored dependency code without updating:

- [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md)
- [docs-dev/robustness/dependency-inventory.json](../docs-dev/robustness/dependency-inventory.json)
- [docs/licensing.md](licensing.md), if the license position changes
- Release packaging checks, if notices or source obligations change

## Current Dependencies

| Dependency | Manifest source | Vendored source | Version | License |
|---|---|---|---:|---|
| `{fmt}` | `src/vcpkg.json` dependency `fmt` | `src/fmt`, `src/fmt.cc`, `src/format.cc`, `src/os.cc` | 10.1.1 | MIT with optional compiled-object exception |
| JsonCpp | `src/vcpkg.json` dependency `jsoncpp` | `src/json` | 1.9.5 | MIT or public domain where recognized |

The DLL currently builds `{fmt}` header-only through `FMT_HEADER_ONLY`. JsonCpp headers are available locally under `src/json`, while the library is linked from the vcpkg static package.

## Enforcement

Run:

```powershell
./scripts/ci/check-dependency-inventory.ps1
```

The script verifies:

- Root `LICENSE` is GPL-2.0.
- `src/vcpkg.json` still declares `fmt` and `jsoncpp` at the recorded baseline.
- Vendored `{fmt}` and JsonCpp header versions match the inventory.
- `THIRD_PARTY_NOTICES.md` and release packaging include third-party notices.

The build workflow runs the same check after vcpkg setup.

## Updater

The Windows updater currently uses only .NET SDK/framework libraries and has no explicit NuGet package dependencies. If NuGet packages are added, they must be added to the dependency inventory and notices before release.
