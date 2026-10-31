# Kane & Lynch: Dead Men - Stutter Fix

Fixes the stutter in *Kane & Lynch: Dead Men* on PC (GOG, Steam and retail), and makes the
HUD, subtitles and menus readable at high resolutions such as 4K.

## What it fixes

- **The hitch every 2 seconds.** The game rescans every input device on the PC every
  2 seconds looking for new controllers. On modern Windows each scan freezes the game
  for 100-200 ms, even with VSync or a frame cap.
- **Uneven frame timing.** The per-frame delta time was truncated to ~1 ms steps; it is
  now computed at full precision, making motion more even.
- **Tiny HUD, subtitles and menus at high resolutions.** The game draws its 2D interface
  at native pixel size, so at 4K everything is half the size it is at 1080p. The HUD,
  subtitles and menus are now scaled with the screen height (2x at 4K, 1.33x at 1440p),
  so they look as they do at 1080p. Mouse clicks in the menus still land correctly.

`kaneandlynch.exe` is **not** modified. The fix is a `dinput8.dll` placed in the game
folder that patches the game's code in memory at startup. This matters because Games for
Windows LIVE (Steam/retail) validates the exe's signature and refuses to start a modified
one.

## Install

1. Download `KL_StutterFix_v2.1.zip` from [Releases](../../releases) and extract it.
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
- Sizes can be changed in `KL_StutterFix.ini` in the game folder (created on install):

  ```ini
  [Display]
  HudScale=auto       ; auto = screen height / 1080, or a number from 0.5 to 4
  SubtitleScale=auto  ; auto = same as HudScale
  MenuScale=auto      ; 1 = off
  ```
- If you get "Access denied", run the installer as administrator (needed when the game is
  installed under Program Files).
- Version 1.0 modified the exe on disk, which broke the Steam version. Installing 2.x
  restores the original exe automatically.

## How it works

All patches are located by byte pattern (see [`kl_patches.h`](kl_patches.h),
[`kl_hud.h`](kl_hud.h) and [`kl_menu.h`](kl_menu.h)), so any build containing the same code
is supported; otherwise nothing is changed.

| Patch | Location (GOG/Steam exe) | Change |
|---|---|---|
| `no-rescan` | `ZSysInputWintel::Update`, `0x44A3C0` | The periodic `IDirectInput8::EnumDevices` rescan (every `0x800` ticks = 2.0 s) is skipped. The scan at startup is kept. |
| `frame-dt` | frame tick, `0x43D896` | The frame delta uses the engine's precise QueryPerformanceCounter delta instead of `(ticks_now - ticks_prev) / 1024`. It stays 0 while paused. |
| `hud-scale` | on-screen display, `0x658EA0` | The HUD scale global (`0xA47CEC`) is 1.0 above 1024 px wide; it becomes `HudScale`. |
| `subtitles` | subtitle layout, `0x65CE00` | Replaced: the subtitle lines get `SubtitleScale` and the word-wrap width is divided by it so lines still fit. |
| `menu` | menu framework screen-size reads; projection builder `0x486060`; GUI camera setup `0x57C540` | Menus are laid out on a virtual screen of `width / MenuScale` by `height / MenuScale` (1920x1080 at 4K), and the projection of the GUI cameras (background, menu, dialog, TRC, mouse) is stretched so that area covers the whole screen. The mouse cursor and hit tests stay in layout space. |

## Building

Requires the Visual Studio Build Tools (MSVC, x86). Run `build.bat`; it produces
`build\dinput8.dll` and `build\KL_StutterFix.exe` (with the DLL embedded).

| File | Purpose |
|---|---|
| `kl_patches.h` | Patch patterns, shared by the DLL and the installer |
| `kl_hud.h`, `kl_menu.h` | HUD, subtitle and menu scale hooks (DLL only) |
| `dinput8_proxy.c`, `dinput8.def`, `dinput8_proxy.rc` | In-memory patch DLL / DirectInput proxy |
| `kl_stutter_fix.c`, `kl_stutter_fix.rc` | Installer |
| `README.txt` | Readme shipped in the release zip |

## License

[MIT](LICENSE)
