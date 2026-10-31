# Kane & Lynch: Dead Men - KL Fix

Fixes the stutter in *Kane & Lynch: Dead Men* on PC (GOG, Steam and retail), makes the
HUD, subtitles and menus readable at high resolutions such as 4K, and removes the need
for Games for Windows LIVE on the Steam and retail versions.

*Formerly **KL Stutter Fix**. Installing 3.0 over Stutter Fix 2.x updates it in place and
keeps your settings (`KL_StutterFix.ini` becomes `KL_Fix.ini`).*

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
- **Games for Windows LIVE (Steam/retail only).** These versions need the discontinued
  GFWL client installed and signed in. If the game folder has no `xlive.dll` of its own,
  the installer adds an offline `xlive.dll` (built from the open-source
  [XLiveLess](THIRD_PARTY_NOTICES.md)), so GFWL is not needed. The GOG version already
  ships its own offline `xlive.dll` and is left as is. Single-player and local
  split-screen co-op work; online multiplayer and GFWL achievements do not.

`kaneandlynch.exe` is **not** modified. The fix is a `dinput8.dll` placed in the game
folder that patches the game's code in memory at startup. This matters because Games for
Windows LIVE (Steam/retail) validates the exe's signature and refuses to start a modified
one.

## Install

1. Download `KL_Fix_v3.0.zip` from [Releases](../../releases) and extract it.
2. Close the game.
3. Run `KL_Fix.exe`. It finds the game automatically (Steam libraries, GOG,
   retail); if it doesn't, click **Change game folder** and select `kaneandlynch.exe`.
4. Click **Install**. On Steam/retail, untick *Replace Games for Windows LIVE* first if
   you want to keep GFWL.

The window shows what is installed, and lets you change the HUD, subtitle and menu
sizes at any time. To remove the fix, run it again and click **Uninstall**.

Command line (no window): `KL_Fix.exe [--install | --uninstall] [--keep-gfwl] [game folder]`

The installer needs the .NET Framework 4, which is part of Windows 10 and 11.

### Notes

- Connect your controllers **before** starting the game. Controllers plugged in while the
  game is running are not detected.
- Another mod that is also a `dinput8.dll` (e.g. DirectInputFpsFix) is kept as
  `dinput8_next.dll` and still loads.
- `KL_Fix.log` in the game folder shows whether the fix was applied.
- Sizes are set in the installer window, or in `KL_Fix.ini` in the game folder:

  ```ini
  [Display]
  HudScale=auto       ; auto = screen height / 1080, or a number from 0.5 to 4
  SubtitleScale=auto  ; auto = same as HudScale
  MenuScale=auto      ; 1 = off
  ```
- If the game folder needs administrator rights (e.g. under Program Files), the
  installer offers to restart itself as administrator.
- Version 1.0 modified the exe on disk, which broke the Steam version. Installing 2.x
  or later restores the original exe automatically.
- The offline `xlive.dll` keeps its profile settings in
  `%LOCALAPPDATA%\kaneandlynch\KL_NoGFWL`. Progress saved under a GFWL account may not
  carry over; uninstalling the fix brings GFWL back and leaves all saves untouched.
  Back up `%LOCALAPPDATA%\kaneandlynch` before switching if your progress matters.
- An `xlive.dll` that is not this fix's (GOG's, or another mod's) is never replaced or
  removed.

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
`build\dinput8.dll`, `build\xlive.dll` and `build\KL_Fix.exe` (with both DLLs
embedded). The installer is C# / WPF, compiled with the `csc.exe` of the .NET Framework 4
that ships with Windows. `build.bat test` also runs the `xlive.dll` API tests.

| File | Purpose |
|---|---|
| `kl_patches.h` | Patch patterns (the installer has a copy in `installer\Core.cs`) |
| `kl_hud.h`, `kl_menu.h` | HUD, subtitle and menu scale hooks (DLL only) |
| `dinput8_proxy.c`, `dinput8.def`, `dinput8_proxy.rc` | In-memory patch DLL / DirectInput proxy |
| `xlive\` | Offline GFWL replacement (XLiveLess, see [third-party notices](THIRD_PARTY_NOTICES.md)) and its tests |
| `installer\Core.cs` | Installer: game discovery, state, install / uninstall, settings |
| `installer\MainWindow.cs`, `installer\Program.cs` | Installer window and command line |
| `README.txt` | Readme shipped in the release zip |

## License

[MIT](LICENSE). `xlive\` is adapted from
[Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader) (MIT,
[notice](licenses/Ultimate-ASI-Loader-MIT.txt)); see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
