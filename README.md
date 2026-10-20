# DiRT Rally 2.0 — VR Grass Flicker Fix

A DirectX 11 patch for grass that flashes or disappears in VR.

Disables the problematic camera-frustum check during grass generation and keeps the working UV-based fade and buffer guards. Enabled automatically; no settings or logging.

## Install

1. Close the game.
2. Download the ZIP from [Releases](https://github.com/armax9/dirt-rally-2-vr-grass-fix/releases).
3. Extract `version.dll` beside `dirtrally2.exe`. Back up an existing `version.dll` first.
4. In CrossOver/Wine, add a `version` library override set to **Native, then Builtin** for `dirtrally2.exe`.
5. Launch the game as usual.

Remove `version.dll` to uninstall, or restore your previous copy.

## Compatibility

Confirmed working with **CrossOver Preview + D3DMetal + MoltenVR + OpenComposite**. Windows and DXVK are untested.

This patches specific game code and shaders. Only recognized game code and shaders are patched. It does not change headset tracking, eye projection, FOV or IPD.

## Build

Requires an x86-64 Windows llvm-mingw toolchain on `PATH`:

```sh
sh build.sh
```

## License

LGPL-2.1-or-later. See `LICENSE` and `THIRD_PARTY_NOTICES.txt`.
