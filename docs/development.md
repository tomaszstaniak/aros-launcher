---
type: guide
updated: 2026-11-17
---

# Development

## Supported scope

Builds and has been run on AROS x86_64 ABIv1 (mainline), AROS x86_64 ABIv11 and AROS Raspberry Pi AArch64 ABIv1. No release yet; test builds only.

## Setup and commands

Needs: the AROS cross compiler and SDK of each target (GCC 10.5 for x86_64), a host C and C++ compiler with FreeType (`pkg-config freetype2`) for the host tests, and a checkout of zunekit at the commit named in `zunekit.pin`.

Copy `local.env.example` to `local.env` and fill in the paths, then:

```sh
scripts/test-host.sh                              # host tests of the pure modules
LAUNCHER_TARGET=x86_64-v1 scripts/build.sh        # build/x86_64-v1/Launcher
LAUNCHER_TARGET=x86_64-abiv11 scripts/build.sh    # build/x86_64-abiv11/Launcher
```

More:

```sh
LAUNCHER_TARGET=<target> scripts/build-widgets.sh     # the Currency widget's provider (needs curl and OpenSSL in the SDK)
LAUNCHER_TARGET=<target> LAUNCHER_LABEL=test1 scripts/package.sh    # dist/Launcher-<version>-<label>-<target>.zip
LAUNCHER_TARGET=<target> scripts/package-widget.sh    # dist/Currency-widget-<version>-<target>.zip
```

Targets: `x86_64-v1`, `x86_64-abiv11`, `aarch64-raspi-v1` (the Pi compiler wrapper takes its SDK from `RPI_SDK`).

Machine values belong in ignored local configuration. The versioned
[configuration example](../local.env.example) documents the selected interface;
[workflow scripts](../scripts/README.md) describe the loader contract. Do not publish
actual workstation paths or credentials. Public instructions must not depend on
private reports or guides in `<workspace>/docs/`.
