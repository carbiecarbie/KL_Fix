# Kane & Lynch: Dead Men - Stutter Fix

Fixes the stutter in *Kane & Lynch: Dead Men* on PC (GOG, Steam and retail).

## What it fixes

- **The hitch every 2 seconds.** The game rescans every input device on the PC every
  2 seconds looking for new controllers. On modern Windows each scan freezes the game
  for 100-200 ms, even with VSync or a frame cap.
- **Uneven frame timing.** The per-frame delta time was truncated to ~1 ms steps; it is
  now computed at full precision, making motion more even.

`kaneandlynch.exe` is **not** modified. The fix is a `dinput8.dll` placed in the game
folder that patches the game's code in memory at startup. This matters because Games for
Windows LIVE (Steam/retail) validates the exe's signature and refuses to start a modified
one.

## Install

1. Download `KL_StutterFix_v2.0.zip` from [Releases](../../releases) and extract it.
2. Close the game.
3. Run `KL_StutterFix.exe`. It finds the game automatically. If it doesn't, put
   `KL_StutterFix.exe` in the game folder, or drag `kaneandlynch.exe` onto it.
4. Choose **[1] Install the fix**.

To remove it, run `KL_StutterFix.exe` again and choose **[2] Uninstall**.

Command line: `KL_StutterFix.exe [--install | --uninstall] [game folder]`

### Notes

- Connect your controllers **before** starting the game. Controllers plugged in while the
  game is running are not detected.
- Another mod that is also a `dinput8.dll` (e.g. DirectInputFpsFix) is kept as
  `dinput8_next.dll` and still loads.
- `KL_StutterFix.log` in the game folder shows whether the fix was applied.
- If you get "Access denied", run the installer as administrator (needed when the game is
  installed under Program Files).
- Version 1.0 modified the exe on disk, which broke the Steam version. Installing 2.0
  restores the original exe automatically.

## How it works

Both patches are located by byte pattern (see [`kl_patches.h`](kl_patches.h)), so any
build containing the same code is supported; otherwise nothing is changed.

| Patch | Location (GOG/Steam exe) | Change |
|---|---|---|
| `no-rescan` | `ZSysInputWintel::Update`, `0x44A3C0` | The periodic `IDirectInput8::EnumDevices` rescan (every `0x800` ticks = 2.0 s) is skipped. The scan at startup is kept. |
| `frame-dt` | frame tick, `0x43D896` | The frame delta uses the engine's precise QueryPerformanceCounter delta instead of `(ticks_now - ticks_prev) / 1024`. It stays 0 while paused. |

## Building

Requires the Visual Studio Build Tools (MSVC, x86). Run `build.bat`; it produces
`build\dinput8.dll` and `build\KL_StutterFix.exe` (with the DLL embedded).

| File | Purpose |
|---|---|
| `kl_patches.h` | Patch patterns, shared by the DLL and the installer |
| `dinput8_proxy.c`, `dinput8.def`, `dinput8_proxy.rc` | In-memory patch DLL / DirectInput proxy |
| `kl_stutter_fix.c`, `kl_stutter_fix.rc` | Installer |
| `README.txt` | Readme shipped in the release zip |

## License

[MIT](LICENSE)
