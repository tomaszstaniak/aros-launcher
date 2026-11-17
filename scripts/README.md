# Workflow scripts

- `env.sh`: sourced by the others. Loads `../local.env`; values already in the environment win; relative values resolve against the directory of `local.env`. Works in bash and zsh.
- `target.sh`: the target table (`LAUNCHER_TARGET`), the compiler/SDK ABI cross-check and the zunekit pin check.
- `build.sh`: builds `build/<target>/Launcher`.
- `test-host.sh`: builds and runs `tests/host/test_*.c` with AddressSanitizer and UBSan.

Commands and configuration keys are described in [development](../docs/development.md).
