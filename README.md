# DiRT Rally 2.0 — VR Grass and Menu Colour Fix

A small DirectX 11 patch for disappearing grass and washed-out VR menus.

## Version 1.2.0

- Corrects an extra `1/2.2` gamma operation in two specific menu pixel shaders. The patch edits the game's supplied bytecode in memory, validates its identity and checksum, and keeps the original shader if validation or creation fails.
- Retains all confirmed grass fixes, including the camera-frustum correction, UV-based fade, bounds guards and distant-instance buffer allocation.
- Clean release: no diagnostic files, status logs, GPU captures or mode switches.

The menu shader identities and gamma operation were cross-checked against [RenoDX's DiRT Rally 2.0 shaders](https://github.com/clshortfuse/renodx/tree/main/src/games/dirtrally2). No complete game shaders or game assets are included.

## Install

1. Close the game.
2. Download the ZIP from [Releases](https://github.com/armax9/dirt-rally-2-vr-grass-fix/releases).
3. Extract `version.dll` beside `dirtrally2.exe`, replacing the previous grass-fix DLL.
4. In CrossOver/Wine, retain a `version` library override set to **Native, then Builtin** for `dirtrally2.exe`.
5. Launch VR through your usual Steam setup.

Remove `version.dll` to uninstall. This DLL does not replace OpenComposite.

If the menu is clipped, set `width="1920" height="1080"` in the resolution entry of `Documents/My Games/DiRT Rally 2.0/hardwaresettings/hardware_settings_config_vr.xml` while the game is closed. This is a separate settings issue; do not replace the entire personal settings file.

## Compatibility

Confirmed with **CrossOver Preview + D3DMetal + MoltenVR + OpenComposite**. Both targeted UI shaders were matched successfully in the tested game build. Windows and DXVK remain untested.

Only recognized game code and shaders are patched. Tracking, FOV, eye projection, IPD and stage lighting are unchanged. Distant vegetation uses a checked 256 MiB total instance-buffer ceiling; increased visible vegetation can increase GPU work.

## Build

Open an **x64 Visual Studio Developer Command Prompt** with the Windows C++ tools installed, then run:

```bat
build-windows.cmd
```

The output is `version.dll`. The release uses MSVC with a static runtime and strict floating-point settings.

## License

LGPL-2.1-or-later. See `LICENSE` and `THIRD_PARTY_NOTICES.txt`.
