# CustomizeNames

Change your player or ArFA-sys gender and name in regular Sword Art Online: Fatal Bullet customization. Keeping the current gender opens your existing look directly in the appearance editor. The name field starts with the current name.

## Build and install

Build with MSVC x64, Git, UE4SS source at `17e30d5c`, and its matching Shipping x64 DLL. Initialize the `deps/first/Unreal` submodule before building.

```powershell
.\build.ps1 -Ue4ssSource 'C:\src\RE-UE4SS' -Ue4ssDll 'C:\game\ue4ss\UE4SS.dll'
```

With the game closed, copy `dist/CustomizeNames` into `SAOFB/Binaries/Win64/ue4ss/Mods`. If you used the earlier BPML workaround, move `BPNewPlayerCustomization.pak` and its matching `.sig` out of `SAOFB/Content/Paks/LogicMods`.

## Credits

- **eluwave** — original Blueprint mod and in-game testing.
- **T3 Code (`gpt-6.2-sol`, medium reasoning)** — UE4SS implementation and build script.

## Community

[SAO Modding Discord](https://discord.gg/7DDZ8G7eH8)
