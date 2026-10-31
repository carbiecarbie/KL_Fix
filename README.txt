Kane & Lynch: Dead Men - KL Fix 3.0
===================================

What it fixes
-------------
- The hitch that happens every 2 seconds, even with VSync or a frame cap.
  The game rescans every input device on the PC every 2 seconds looking for new
  controllers; on modern Windows this freezes the game for 100-200 ms.
- Frame delta time is computed at full precision (it was truncated to ~1 ms steps),
  making motion more even.
- The HUD, subtitles and menus are tiny at high resolutions such as 4K, because
  the game draws them at native pixel size. They are now scaled with the screen
  height (2x at 4K, 1.33x at 1440p), so they look as they do at 1080p.
  Mouse clicks in the menus still land correctly.
- Steam/retail: Games for Windows LIVE is no longer needed. If the game folder has
  no xlive.dll of its own, the installer adds an offline xlive.dll (open-source
  XLiveLess). The GOG version already has one and is left as is.
  Single-player and local split-screen co-op work; online multiplayer and GFWL
  achievements do not.

Works with the GOG and Steam (and retail) versions. kaneandlynch.exe is NOT modified:
the fix is applied in memory by a dinput8.dll placed in the game folder. This is
required because Games for Windows LIVE (Steam/retail) refuses to start a modified exe.

How to install
--------------
1. Close the game.
2. Run KL_Fix.exe. It finds the game automatically. If it doesn't, click
   "Change game folder" and select kaneandlynch.exe.
3. Click Install. On Steam/retail, untick "Replace Games for Windows LIVE" first if
   you want to keep GFWL.

The window shows what is installed and lets you change the HUD, subtitle and menu
sizes at any time. To remove the fix, run it again and click Uninstall.
Requires the .NET Framework 4, which is part of Windows 10 and 11.

Notes
-----
- Connect your controllers BEFORE starting the game. Controllers plugged in while
  the game is running are not detected.
- If you already use another mod that is also a dinput8.dll (e.g. DirectInputFpsFix),
  it is kept as dinput8_next.dll and still loads.
- KL Fix was called KL Stutter Fix up to version 2.1. Installing 3.0 over it updates
  it in place and keeps your settings (KL_StutterFix.ini becomes KL_Fix.ini).
- Used version 1.0 of this fix (which modified the exe)? Just install 3.0: it restores
  the original exe automatically.
- Progress saved under a GFWL account may not carry over to the offline xlive.dll.
  Uninstalling the fix brings GFWL back; no saves are deleted. Back up
  %LOCALAPPDATA%\kaneandlynch before switching if your progress matters.
- KL_Fix.log in the game folder shows whether the fix was applied.
- Sizes are set in the installer window, or in KL_Fix.ini in the game
  folder. Each value is auto (screen height / 1080) or a number from 0.5 to 4:
      HudScale=auto
      SubtitleScale=auto   (auto = same as HudScale)
      MenuScale=auto       (1 = off)
- If the game folder needs administrator rights (e.g. under Program Files), the
  installer offers to restart itself as administrator.

Command line (no window):
  KL_Fix.exe [--install | --uninstall] [--keep-gfwl] [game folder]

Source code is included in the "source" folder. The offline xlive.dll is adapted
from Ultimate ASI Loader's XLiveLess (MIT); see source\THIRD_PARTY_NOTICES.md.
