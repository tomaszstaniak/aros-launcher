# Jumpstart

Jumpstart is a keyboard launcher for AROS. A hotkey opens a panel; a few typed letters find a program, a file, an open window, a calculation, a package or a clipboard entry, and Return acts on it.

[Home page with screenshots](https://tomaszstaniak.github.io/aros-launcher/) · [Releases](https://github.com/tomaszstaniak/aros-launcher/releases)

## What it does

- Starts programs found by a few letters, in order or scattered; brings open windows to the front; opens files from folders you choose.
- Answers calculations, unit and time-zone questions, currencies (European Central Bank rates) and the weather (through Dockbar's Weather widget).
- Tab lists a result's actions, among them an alias or a hotkey of its own.
- Keeps a clipboard history in memory, lists packages from [apkg](https://github.com/tomaszstaniak/arospkg), runs Shell commands, and has an ARexx port.
- Reads colour themes in the format of [Limpet](https://github.com/tomaszstaniak/limpet); fifteen are included.

The user's guide is `packaging/ReadMe.txt`, which is also in every archive.

## Install

With apkg: `apkg install jumpstart`. Or download the archive for your system from the releases page, unpack it anywhere and double-click Jumpstart.

Archives exist for x86_64 ABIv11 distributions, mainline AROS x86_64 (ABIv1) and the Raspberry Pi (AArch64, ABIv1). The binaries do not cross.

## Build

See [development](docs/development.md). The text layer and the frosted-glass panel come from [zunekit](https://github.com/tomaszstaniak/zunekit) at the commit named in `zunekit.pin`.

## Licence

MIT, see `LICENSE`. The Currency widget's provider links curl and OpenSSL; their licence texts are in `widgets/org.aros.currency/licenses`. The colour themes have their own licences, listed in `themes/THEME-LICENSES.txt`.
