Kane & Lynch: Dead Men - Stutter Fix 2.1
========================================

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

Works with the GOG and Steam (and retail) versions. kaneandlynch.exe is NOT modified:
the fix is applied in memory by a dinput8.dll placed in the game folder. This is
required because Games for Windows LIVE (Steam/retail) refuses to start a modified exe.

How to install
--------------
1. Close the game.
2. Run KL_StutterFix.exe. It finds the game automatically.
   If it doesn't, put KL_StutterFix.exe in the game folder, or drag kaneandlynch.exe
   onto it.
3. Choose [1] Install the fix.

To remove it, run KL_StutterFix.exe again and choose [2] Uninstall.

Notes
-----
- Connect your controllers BEFORE starting the game. Controllers plugged in while
  the game is running are not detected.
- If you already use another mod that is also a dinput8.dll (e.g. DirectInputFpsFix),
  it is kept as dinput8_next.dll and still loads.
- Used version 1.0 of this fix (which modified the exe)? Just install 2.1: it restores
  the original exe automatically.
- KL_StutterFix.log in the game folder shows whether the fix was applied.
- Sizes can be changed in KL_StutterFix.ini in the game folder (created on
  install). Each value is auto (screen height / 1080) or a number from 0.5 to 4:
      HudScale=auto
      SubtitleScale=auto   (auto = same as HudScale)
      MenuScale=auto       (1 = off)
- If you get "Access denied", right-click and choose "Run as administrator"
  (needed when the game is installed under Program Files).

Command line: KL_StutterFix.exe [--install | --uninstall] [game folder]

Source code is included in the "source" folder.
