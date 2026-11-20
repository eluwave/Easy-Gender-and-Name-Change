# CustomizeNames

Rename your player or ArFA-sys after confirming their appearance in Sword Art Online: Fatal Bullet. The name field starts with their current name.

**Compatibility** Steam 1.9.0 (Win64) and UE4SS v3.0.1 Beta (`17e30d5c`).

## Build and install

Build with MSVC x64, Git, and the matching UE4SS source and Shipping x64 DLL. Initialize the `deps/first/Unreal` submodule before building.

```powershell
.\build.ps1 -Ue4ssSource 'C:\src\RE-UE4SS' -Ue4ssDll 'C:\game\ue4ss\UE4SS.dll'
```

With the game closed, copy `dist/CustomizeNames` into `SAOFB/Binaries/Win64/ue4ss/Mods`. If you used the earlier BPML workaround, move `BPNewPlayerCustomization.pak` and its matching `.sig` out of `SAOFB/Content/Paks/LogicMods`.

## Credits

- **eluwave** — original Blueprint mod and in-game testing.
- **OpenAI Codex (`gpt-6.2-sol`)** — UE4SS implementation and build script.

## Community

[SAO Modding Discord](https://discord.gg/7DDZ8G7eH8)
