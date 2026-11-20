# CustomizeNames for Sword Art Online Fatal Bullet

This UE4SS mod adds the game's name-entry step after you confirm appearance in the regular Player or ArFA-sys customization menu. The game reads the target character's current name and passes it to the editable `entry_name` row. You can confirm the existing name as-is. Back from the name screen returns to appearance customization.

## Install

1. Build the mod using the steps below.
2. Exit the game. If you installed the earlier BPML workaround, move `BPNewPlayerCustomization.pak` and its matching `.sig` out of `SAOFB/Content/Paks/LogicMods`.
3. Copy `dist/CustomizeNames` into `SAOFB/Binaries/Win64/ue4ss/Mods`. The DLL should be at `ue4ss/Mods/CustomizeNames/dlls/main.dll` and `enabled.txt` should be directly inside `CustomizeNames`.
4. Open appearance customization from the normal in-game menu. Confirm your appearance to open the name field with the current character name filled in.

The mod was tested in-game on the Steam 1.9.0 Win64 executable with UE4SS v3.0.1 Beta, Git `17e30d5c`. `dlls/CustomizeNames.log` reports whether the game code matched this build and the change was applied. To uninstall, exit the game and remove the `CustomizeNames` mod folder.

## Implementation

Both native appearance-confirmation callbacks branch around name entry during repeat customization. The DLL removes those two jumps in memory after verifying the executable and callback bytes. It leaves the normal `FirstTime=0` initialization and return route in use. The native name-entry state takes the current target's name from its character data and seeds `WBP_NameInput` through the `entry_name` grid parameter. The normal commit routine already applies a nonempty entry name before its `partsOnly` check.

## Build

Use the UE4SS source checkout at `17e30d5c` with its `deps/first/Unreal` submodule and the matching Shipping x64 `UE4SS.dll`. The build script requires MSVC x64 and Git for the three pinned header dependencies.

```powershell
.\build.ps1 -Ue4ssSource 'C:\src\RE-UE4SS' -Ue4ssDll 'C:\game\ue4ss\UE4SS.dll'
```

The install folder is written to `dist/CustomizeNames`.

## Credits

- **eluwave** — original Blueprint mod, feature design, and in-game testing.
- **OpenAI Codex** — native UE4SS implementation and build script.

## Community

Discuss this mod and other Fatal Bullet mods on the [SAO Modding Discord](https://discord.gg/7DDZ8G7eH8).
