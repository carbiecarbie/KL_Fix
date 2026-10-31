// Kane & Lynch: Dead Men - KL Fix installer: game discovery, state and
// install/uninstall. No UI here; Program.cs (command line) and MainWindow.cs use it.
//
// The fix is a dinput8.dll proxy (dinput8_proxy.c) that patches the game in memory;
// kaneandlynch.exe is never modified, which keeps its signature valid for Games for
// Windows - LIVE. An existing dinput8.dll wrapper (e.g. DirectInputFpsFix) is renamed
// to dinput8_next.dll and chain-loaded. Version 1.0 patched the exe on disk; installing
// or uninstalling restores it from that version's backup.
//
// Games for Windows - LIVE: the game loads xlive.dll from its own folder first. GOG
// ships an offline xlive.dll there; Steam and retail have none and use the installed
// GFWL. When the folder has no xlive.dll, the offline replacement built from xlive\
// (XLiveLess) is installed unless the user keeps GFWL. An xlive.dll from someone else is
// never replaced; uninstalling removes only ours.
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.RegularExpressions;
using Microsoft.Win32;

namespace KLFix
{
    // A failure with a message meant for the user. NeedsAdmin: retrying elevated may help.
    public class FixException : Exception
    {
        public bool NeedsAdmin;
        public FixException(string message, bool needsAdmin) : base(message) { NeedsAdmin = needsAdmin; }
        public FixException(string message) : this(message, false) { }
    }

    public enum Edition { Steam, Gog, Other }

    // The patch sites of kl_patches.h, used to recognise supported and v1.0-patched exes.
    static class Patterns
    {
        const short W = 0x100;  // any byte
        static readonly short[][] Original = {
            new short[] { 0x83, 0xC1, 0x28, 0x81, 0xC2, 0x00, 0x08, 0x00, 0x00, 0x3B, 0x11, 0x7D, 0x11 },
            new short[] { 0x8B, 0x00, 0x2B, 0x01, 0x5F, 0x89, 0x44, 0x24, 0x04, 0xDB, 0x44, 0x24, 0x04,
                          0xDC, 0x0D, W, W, W, W, 0xD9, 0x5E, 0x50 },
        };
        static readonly short[][] Patched = {
            new short[] { 0x83, 0xC1, 0x28, 0x81, 0xC2, 0x00, 0x08, 0x00, 0x00, 0x3B, 0x11, 0xEB, 0x11 },
            new short[] { 0x8B, 0x00, 0x2B, 0x01, 0x5F, 0xD9, 0xEE, 0x85, 0xC0, 0x74, 0x05, 0xDD, 0xD8,
                          0xD9, 0x46, 0x3C, 0xD9, 0x5E, 0x50, 0x90, 0x90, 0x90 },
        };

        static int Count(byte[] data, short[] pat)
        {
            int count = 0;
            for (int i = 0; i + pat.Length <= data.Length; i++)
            {
                if (data[i] != pat[0]) continue;
                int j = 1;
                while (j < pat.Length && (pat[j] == W || data[i + j] == pat[j])) j++;
                if (j == pat.Length) count++;
            }
            return count;
        }

        // False if a site is missing or ambiguous; otherwise counts original/patched sites.
        public static bool Scan(byte[] data, out int original, out int patched)
        {
            original = patched = 0;
            for (int i = 0; i < Original.Length; i++)
            {
                int o = Count(data, Original[i]), p = Count(data, Patched[i]);
                if (o + p != 1) return false;
                original += o;
                patched += p;
            }
            return true;
        }

        public static int SiteCount { get { return Original.Length; } }
    }

    public class GameState
    {
        public const string Version = "3.0";
        // UTF-16, in the DLL's version info; versions up to 2.1 were named KL_StutterFix.
        static readonly string[] DllMarkers = { "KL_Fix_dinput8", "KL_StutterFix_dinput8" };
        const string XliveMarker = "KL NoGFWL";   // ASCII, in our xlive.dll

        public string Folder, Exe, Dll, Chain, Log, Ini, Xlive;
        public string OldLog, OldIni;    // KL_StutterFix.log / .ini of versions up to 2.1
        public string NogfwlRecord, NogfwlDir, NogfwlBackup;  // standalone KL NoGFWL 0.1
        public string V1Backup;          // original exe saved by version 1.0, if usable
        public bool DllOld;              // ours, but an older version (Stutter Fix 2.x)
        public bool ExeSupported, ExePatched, DllOurs, DllForeign, ChainPresent, XliveOurs, XliveForeign;
        public Edition Edition;

        public bool Installed { get { return DllOurs || XliveOurs; } }
        // The settings file in use: the old name until an older version is updated.
        public string SettingsFile { get { return !File.Exists(Ini) && File.Exists(OldIni) ? OldIni : Ini; } }
        // The game would use the real GFWL, so the user can choose to replace it.
        public bool GfwlChoice { get { return !XliveOurs && !XliveForeign; } }

        public static GameState Load(string folder)
        {
            var st = new GameState();
            st.Folder = folder;
            st.Exe = Path.Combine(folder, "kaneandlynch.exe");
            st.Dll = Path.Combine(folder, "dinput8.dll");
            st.Chain = Path.Combine(folder, "dinput8_next.dll");
            st.Log = Path.Combine(folder, "KL_Fix.log");
            st.Ini = Path.Combine(folder, "KL_Fix.ini");
            st.OldLog = Path.Combine(folder, "KL_StutterFix.log");
            st.OldIni = Path.Combine(folder, "KL_StutterFix.ini");
            st.Xlive = Path.Combine(folder, "xlive.dll");
            st.NogfwlRecord = Path.Combine(folder, "KL_NoGFWL.install.json");
            st.NogfwlDir = Path.Combine(folder, "KL_NoGFWL.backup");
            st.NogfwlBackup = Path.Combine(st.NogfwlDir, "xlive.dll");

            byte[] exe = ReadAll(st.Exe);
            int original, patched;
            st.ExeSupported = Patterns.Scan(exe, out original, out patched);
            st.ExePatched = st.ExeSupported && patched > 0;
            st.V1Backup = FindV1Backup(st.Exe);

            if (File.Exists(st.Dll))
            {
                st.DllOurs = Contains(st.Dll, Encoding.Unicode.GetBytes(DllMarkers[0]));
                st.DllOld = !st.DllOurs && Contains(st.Dll, Encoding.Unicode.GetBytes(DllMarkers[1]));
                st.DllOurs |= st.DllOld;
                st.DllForeign = !st.DllOurs;
            }
            st.ChainPresent = File.Exists(st.Chain);
            if (File.Exists(st.Xlive))
            {
                st.XliveOurs = Contains(st.Xlive, Encoding.ASCII.GetBytes(XliveMarker));
                st.XliveForeign = !st.XliveOurs;
            }

            if (Directory.GetFiles(folder, "goggame-*.info").Length > 0) st.Edition = Edition.Gog;
            else if (File.Exists(Path.Combine(folder, "installscript.vdf")) ||
                     folder.IndexOf("steamapps", StringComparison.OrdinalIgnoreCase) >= 0) st.Edition = Edition.Steam;
            else st.Edition = Edition.Other;
            return st;
        }

        static byte[] ReadAll(string path)
        {
            try { return File.ReadAllBytes(path); }
            catch (IOException e) { throw new FixException("Could not read " + Path.GetFileName(path) + ": " + e.Message); }
            catch (UnauthorizedAccessException) { throw new FixException("Access to " + path + " was denied.", true); }
        }

        static bool Contains(string path, byte[] marker)
        {
            byte[] data;
            try { data = File.ReadAllBytes(path); } catch (Exception) { return false; }
            for (int i = 0; i + marker.Length <= data.Length; i++)
            {
                if (data[i] != marker[0]) continue;
                int j = 1;
                while (j < marker.Length && data[i + j] == marker[j]) j++;
                if (j == marker.Length) return true;
            }
            return false;
        }

        // A version 1.0 backup is only usable if it holds the fully original code.
        static string FindV1Backup(string exe)
        {
            foreach (string suffix in new[] { ".stutterfix-backup", ".orig" })
            {
                string path = exe + suffix;
                if (!File.Exists(path)) continue;
                try
                {
                    int original, patched;
                    if (Patterns.Scan(File.ReadAllBytes(path), out original, out patched) &&
                        original == Patterns.SiteCount) return path;
                }
                catch (Exception) { }
            }
            return null;
        }
    }

    public static class Installer
    {
        public const string DefaultIni =
            "; Kane & Lynch: Dead Men - KL Fix " + GameState.Version + " settings\r\n" +
            "; Sizes of the HUD, subtitles and menus at high resolutions.\r\n" +
            "; auto = screen height / 1080 (2.0 at 4K, 1.33 at 1440p: they look as at 1080p).\r\n" +
            "; Or a number from 0.5 to 4. Changes apply the next time the game starts.\r\n" +
            "[Display]\r\n" +
            "HudScale=auto\r\n" +
            "; auto = same as HudScale\r\n" +
            "SubtitleScale=auto\r\n" +
            "; 1 = off (menus at native pixel size)\r\n" +
            "MenuScale=auto\r\n";

        // ---- Game discovery --------------------------------------------------------

        // Accepts a folder or a path to kaneandlynch.exe; returns the game folder or null.
        public static string Resolve(string input)
        {
            if (string.IsNullOrEmpty(input)) return null;
            string path = input.Trim().Trim('"').TrimEnd('\\');
            try
            {
                if (File.Exists(path))
                {
                    if (!string.Equals(Path.GetFileName(path), "kaneandlynch.exe", StringComparison.OrdinalIgnoreCase)) return null;
                    return Path.GetDirectoryName(Path.GetFullPath(path));
                }
                if (path.Length > 0 && File.Exists(Path.Combine(path, "kaneandlynch.exe"))) return Path.GetFullPath(path);
            }
            catch (Exception) { }
            return null;
        }

        static string RegValue(RegistryHive hive, string key, string name)
        {
            foreach (RegistryView view in new[] { RegistryView.Registry32, RegistryView.Registry64 })
            {
                try
                {
                    using (var root = RegistryKey.OpenBaseKey(hive, view))
                    using (var k = root.OpenSubKey(key))
                    {
                        var value = k == null ? null : k.GetValue(name) as string;
                        if (!string.IsNullOrEmpty(value)) return value;
                    }
                }
                catch (Exception) { }
            }
            return null;
        }

        static IEnumerable<string> Candidates(string[] args)
        {
            foreach (string a in args) if (!a.StartsWith("--")) yield return a;
            yield return AppDomain.CurrentDomain.BaseDirectory;
            yield return Environment.CurrentDirectory;
            // Installers: retail/GOG/Steam register an Eidos key, GOG also its own key.
            yield return RegValue(RegistryHive.LocalMachine, @"SOFTWARE\Eidos\Kane and Lynch Dead Men", "InstallationPath");
            yield return RegValue(RegistryHive.LocalMachine, @"SOFTWARE\GOG.com\Games\1721096025", "path");
            // Steam: every library folder that holds app 8080.
            var steam = new List<string>();
            steam.Add(RegValue(RegistryHive.CurrentUser, @"Software\Valve\Steam", "SteamPath"));
            steam.Add(RegValue(RegistryHive.LocalMachine, @"SOFTWARE\Valve\Steam", "InstallPath"));
            var libraries = new List<string>();
            foreach (string root in steam)
            {
                if (string.IsNullOrEmpty(root)) continue;
                libraries.Add(root);
                try
                {
                    string vdf = Path.Combine(root, @"steamapps\libraryfolders.vdf");
                    if (File.Exists(vdf))
                        foreach (Match m in Regex.Matches(File.ReadAllText(vdf), "\"path\"\\s*\"([^\"]+)\""))
                            libraries.Add(m.Groups[1].Value.Replace(@"\\", @"\"));
                }
                catch (Exception) { }
            }
            foreach (string lib in libraries)
                yield return Path.Combine(lib.Replace('/', '\\'), @"steamapps\common\Kane and Lynch Dead Men");
        }

        public static string FindGame(string[] args)
        {
            foreach (string c in Candidates(args))
            {
                string folder = Resolve(c);
                if (folder != null) return folder;
            }
            return null;
        }

        // ---- File helpers ------------------------------------------------------------

        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        static extern bool MoveFileExW(string from, string to, int flags);
        const int MOVEFILE_REPLACE_EXISTING = 1, MOVEFILE_WRITE_THROUGH = 8;

        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        static extern bool WritePrivateProfileStringW(string section, string key, string value, string path);
        [DllImport("kernel32.dll", CharSet = CharSet.Unicode)]
        static extern int GetPrivateProfileStringW(string section, string key, string def, StringBuilder ret, int size, string path);

        static FixException Explain(Exception e, string path)
        {
            int hr = Marshal.GetHRForException(e) & 0xFFFF;
            if (e is UnauthorizedAccessException || hr == 5)
                return new FixException("Windows denied access to " + Path.GetFileName(path) + ". The game folder needs administrator rights.", true);
            if (hr == 32 || hr == 33)
                return new FixException("The game seems to be running. Close it and try again.");
            return new FixException(Path.GetFileName(path) + ": " + e.Message);
        }

        static void Win32Move(string from, string to, int flags)
        {
            if (MoveFileExW(from, to, flags)) return;
            int err = Marshal.GetLastWin32Error();
            if (err == 5) throw new FixException("Windows denied access to " + Path.GetFileName(to) + ". The game folder needs administrator rights.", true);
            if (err == 32 || err == 33) throw new FixException("The game seems to be running. Close it and try again.");
            throw new FixException(Path.GetFileName(to) + ": Windows error " + err);
        }

        // Write via a temporary file so a failure never leaves a half-written file.
        static void WriteAtomic(string path, byte[] data)
        {
            string tmp = path + ".tmp";
            try
            {
                using (var f = new FileStream(tmp, FileMode.Create, FileAccess.Write, FileShare.None))
                {
                    f.Write(data, 0, data.Length);
                    f.Flush(true);
                }
                Win32Move(tmp, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
            }
            catch (FixException) { TryDelete(tmp); throw; }
            catch (Exception e) { TryDelete(tmp); throw Explain(e, path); }
        }

        static void TryDelete(string path) { try { File.Delete(path); } catch (Exception) { } }

        static void Delete(string path)
        {
            try { if (File.Exists(path)) File.Delete(path); }
            catch (Exception e) { throw Explain(e, path); }
        }

        // Fails, with the reason, if the file is in use or not writable.
        static void CheckWritable(string path)
        {
            if (!File.Exists(path)) return;
            try { using (new FileStream(path, FileMode.Open, FileAccess.ReadWrite, FileShare.None)) { } }
            catch (Exception e) { throw Explain(e, path); }
        }

        static byte[] Embedded(string name)
        {
            using (var s = typeof(Installer).Assembly.GetManifestResourceStream(name))
            {
                if (s == null) throw new FixException("Internal error: embedded " + name + " is missing.");
                var data = new byte[s.Length];
                int got = 0;
                while (got < data.Length) got += s.Read(data, got, data.Length - got);
                return data;
            }
        }

        public static bool GameRunning()
        {
            var p = Process.GetProcessesByName("kaneandlynch");
            foreach (var x in p) x.Dispose();
            return p.Length > 0;
        }

        // ---- Settings (KL_Fix.ini, [Display]) ----------------------------------

        public static string ReadSetting(GameState st, string key)
        {
            var sb = new StringBuilder(64);
            GetPrivateProfileStringW("Display", key, "auto", sb, sb.Capacity, st.SettingsFile);
            string v = sb.ToString().Trim();
            int semi = v.IndexOf(';');
            if (semi >= 0) v = v.Substring(0, semi).Trim();
            return v.Length == 0 ? "auto" : v;
        }

        public static void WriteSetting(GameState st, string key, string value)
        {
            string ini = st.DllOld ? st.SettingsFile : st.Ini;  // an older DLL reads the old name
            if (!File.Exists(ini)) WriteAtomic(ini, Encoding.ASCII.GetBytes(DefaultIni));
            if (!WritePrivateProfileStringW("Display", key, value, ini))
                throw new FixException("Could not save " + Path.GetFileName(ini) + " (Windows error " + Marshal.GetLastWin32Error() + ").",
                                       Marshal.GetLastWin32Error() == 5);
        }

        // ---- Install / uninstall --------------------------------------------------------

        // Undoes version 1.0's on-disk patch so the signed exe is original again.
        static void RestoreV1Exe(GameState st, List<string> log)
        {
            if (!st.ExePatched) return;
            if (st.V1Backup == null)
            {
                log.Add("kaneandlynch.exe was modified by version 1.0 and no backup was found. " +
                        "On Steam use \"Verify integrity of game files\" to restore it.");
                return;
            }
            WriteAtomic(st.Exe, File.ReadAllBytes(st.V1Backup));
            log.Add("The original kaneandlynch.exe was restored.");
            // Only the backup this tool created is removed.
            if (st.V1Backup.EndsWith(".stutterfix-backup", StringComparison.OrdinalIgnoreCase)) TryDelete(st.V1Backup);
            st.ExePatched = false;
        }

        public static List<string> Install(GameState st, bool keepGfwl)
        {
            var log = new List<string>();
            if (!st.ExeSupported && !st.Installed)
                throw new FixException("This version of kaneandlynch.exe is not supported. Nothing was changed.");
            if (GameRunning()) throw new FixException("The game is running. Close it and try again.");
            byte[] dll = Embedded("dinput8.dll");
            CheckWritable(st.Exe);
            CheckWritable(st.Dll);
            if (st.DllForeign && st.ChainPresent)
                throw new FixException("Both dinput8.dll and dinput8_next.dll already exist from other mods. Remove one of them and try again.");
            RestoreV1Exe(st, log);

            if (st.DllForeign)
            {
                Win32Move(st.Dll, st.Chain, MOVEFILE_WRITE_THROUGH);
                log.Add("The existing dinput8.dll mod was kept as dinput8_next.dll and still loads.");
                st.ChainPresent = true;
                st.DllForeign = false;
            }
            WriteAtomic(st.Dll, dll);
            st.DllOurs = true;
            st.DllOld = false;
            // Stutter Fix 2.x files: keep the settings under the new name, drop the old log.
            if (!File.Exists(st.Ini) && File.Exists(st.OldIni)) Win32Move(st.OldIni, st.Ini, MOVEFILE_WRITE_THROUGH);
            Delete(st.OldLog);
            if (!File.Exists(st.Ini)) WriteAtomic(st.Ini, Encoding.ASCII.GetBytes(DefaultIni));

            if (!st.XliveForeign && !(keepGfwl && !st.XliveOurs))
            {
                CheckWritable(st.Xlive);
                bool fresh = !st.XliveOurs;
                WriteAtomic(st.Xlive, Embedded("xlive.dll"));
                st.XliveOurs = true;
                if (fresh) log.Add("Games for Windows LIVE was replaced by an offline xlive.dll.");
            }
            return log;
        }

        public static List<string> Uninstall(GameState st)
        {
            var log = new List<string>();
            if (GameRunning()) throw new FixException("The game is running. Close it and try again.");
            CheckWritable(st.Exe);
            CheckWritable(st.Dll);
            CheckWritable(st.Xlive);
            if (st.DllOurs)
            {
                Delete(st.Dll);
                st.DllOurs = false;
                if (st.ChainPresent)
                {
                    Win32Move(st.Chain, st.Dll, MOVEFILE_WRITE_THROUGH);
                    st.ChainPresent = false;
                    st.DllForeign = true;
                    log.Add("The previous dinput8.dll mod was restored.");
                }
            }
            if (st.XliveOurs)
            {
                Delete(st.Xlive);
                st.XliveOurs = false;
                if (File.Exists(st.NogfwlBackup))
                {
                    Win32Move(st.NogfwlBackup, st.Xlive, MOVEFILE_WRITE_THROUGH);
                    st.XliveForeign = true;
                    log.Add("The previous xlive.dll was restored.");
                }
                else
                {
                    log.Add("The game uses Games for Windows LIVE again; it must be installed for the game to start.");
                }
                try { Directory.Delete(st.NogfwlDir); } catch (Exception) { }  // only if empty
                TryDelete(st.NogfwlRecord);
            }
            Delete(st.Log);
            Delete(st.Ini);
            Delete(st.OldLog);
            Delete(st.OldIni);
            RestoreV1Exe(st, log);
            return log;
        }
    }
}
