---
type: guide
updated: 2026-11-17
---

# Development

## Supported scope

Intended: AROS x86_64 ABIv1, AROS x86_64 ABIv11, AROS Raspberry Pi AArch64 ABIv1. The program builds for the two x86_64 targets; nothing has been run on AROS yet.

## Setup and commands

Needs: the AROS cross compiler and SDK of each target (GCC 10.5 for x86_64), a host C and C++ compiler with FreeType (`pkg-config freetype2`) for the host tests, and a checkout of zunekit at the commit named in `zunekit.pin`.

Copy `local.env.example` to `local.env` and fill in the paths, then:

```sh
scripts/test-host.sh                              # host tests of the pure modules
LAUNCHER_TARGET=x86_64-v1 scripts/build.sh        # build/x86_64-v1/Launcher
LAUNCHER_TARGET=x86_64-abiv11 scripts/build.sh    # build/x86_64-abiv11/Launcher
```

Packaging and tests on AROS are not implemented yet.

Machine values belong in ignored local configuration. The versioned
[configuration example](../local.env.example) documents the selected interface;
[workflow scripts](../scripts/README.md) describe the loader contract. Do not publish
actual workstation paths or credentials. Public instructions must not depend on
private reports or guides in `<workspace>/docs/`.
