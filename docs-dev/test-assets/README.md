# Robustness Test Asset Pack

This directory contains only redistributable manifests and seed inputs for robustness testing. Do not commit Quake II Rerelease game data, local save files from commercial assets, generated logs, compiled binaries or machine-specific paths.

## External Asset Root

Set `MUFFMODE_TEST_ASSET_ROOT` to a directory outside the repository. A complete phase-zero pack should use this layout:

```text
%MUFFMODE_TEST_ASSET_ROOT%/
  clean-install/
    README.txt
    <legitimate Quake II Rerelease installation files>
  smoke/
    server-headless-smoke.cfg
  corpus/
    command-sequences/
      phase-zero-smoke.commands
      phase-three-regression.commands
    rotation/
      basic-rotation.txt
    save-load/
      synthetic-minimal-save.json
    red-rover/
      0.36.04-regressions.json
```

The `clean-install` directory must come from a legitimate user-owned Quake II Rerelease installation and must not be redistributed through this repository.

Validate the repo-side seeds in CI-compatible mode:

```powershell
./scripts/ci/check-test-assets.ps1 -RepoOnly
```

Validate a complete external pack:

```powershell
$env:MUFFMODE_TEST_ASSET_ROOT = "E:\MuffModeTestAssets"
./scripts/ci/check-test-assets.ps1
```

## Repo-Side Seeds

The checked-in seeds under this directory are safe to redistribute and can be copied into the external asset root:

- `configs/server-headless-smoke.cfg`
- `command-sequences/phase-zero-smoke.commands`
- `command-sequences/phase-three-regression.commands`
- `rotation/basic-rotation.txt`
- `save-load/synthetic-minimal-save.json`
- `red-rover/0.36.04-regressions.json`
- `fuzz-corpus/numeric-parsers/valid-and-invalid.txt`
- `fuzz-corpus/gt-cfg/maxclients-lines.txt`

The checked-in save/load seed is synthetic and is not a real game save. Keep real game-derived saves in the external asset root unless their redistribution status has been reviewed.
