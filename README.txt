Kane & Lynch: Dead Men - Stutter Fix 2.0
========================================

What it fixes
-------------
- The hitch that happens every 2 seconds, even with VSync or a frame cap.
  The game rescans every input device on the PC every 2 seconds looking for new
  controllers; on modern Windows this freezes the game for 100-200 ms.
- Frame delta time is computed at full precision (it was truncated to ~1 ms steps),
  making motion more even.

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
- Used version 1.0 of this fix (which modified the exe)? Just install 2.0: it restores
  the original exe automatically.
- KL_StutterFix.log in the game folder shows whether the fix was applied.
- If you get "Access denied", right-click and choose "Run as administrator"
  (needed when the game is installed under Program Files).

Command line: KL_StutterFix.exe [--install | --uninstall] [game folder]

Source code is included in the "source" folder.
