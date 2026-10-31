// Entry point. Without --install/--uninstall the window opens; with them the installer
// runs without UI and prints to the console it was started from.
//
// Usage: KL_Fix.exe [--install | --uninstall] [--keep-gfwl] [game folder or kaneandlynch.exe]
using System;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Windows;

[assembly: AssemblyTitle("Kane & Lynch: Dead Men - KL Fix")]
[assembly: AssemblyProduct("Kane & Lynch: Dead Men - KL Fix")]
[assembly: AssemblyDescription("Kane & Lynch: Dead Men - KL Fix installer")]
[assembly: AssemblyCopyright("MIT License")]
[assembly: AssemblyVersion("3.0.0.0")]
[assembly: AssemblyFileVersion("3.0.0.0")]

namespace KLFix
{
    static class Program
    {
        [DllImport("kernel32.dll")]
        static extern bool AttachConsole(int pid);

        [STAThread]
        static int Main(string[] args)
        {
            int mode = 0;  // 0 = window, 1 = install, 2 = uninstall
            bool keepGfwl = false;
            foreach (string a in args)
            {
                string s = a.ToLowerInvariant();
                if (s == "--install" || s == "--apply") mode = 1;
                else if (s == "--uninstall" || s == "--restore") mode = 2;
                else if (s == "--keep-gfwl") keepGfwl = true;
            }
            if (mode == 0)
            {
                var app = new Application();
                app.ShutdownMode = ShutdownMode.OnMainWindowClose;
                var window = new MainWindow(Installer.FindGame(args));
                return app.Run(window.Window);
            }
            return RunCommandLine(args, mode == 1, keepGfwl);
        }

        static int RunCommandLine(string[] args, bool install, bool keepGfwl)
        {
            AttachConsole(-1);
            Console.WriteLine();
            Console.WriteLine("Kane & Lynch: Dead Men - KL Fix " + GameState.Version);
            string folder = Installer.FindGame(args);
            if (folder == null)
            {
                Console.WriteLine("Game not found. Pass the game folder as an argument.");
                return 1;
            }
            Console.WriteLine("Game: " + folder);
            try
            {
                var st = GameState.Load(folder);
                var log = install ? Installer.Install(st, keepGfwl) : Installer.Uninstall(st);
                foreach (string line in log) Console.WriteLine(line);
                Console.WriteLine(install ? "KL Fix is installed." : "KL Fix was removed.");
                return 0;
            }
            catch (FixException e)
            {
                Console.WriteLine(e.Message);
                if (e.NeedsAdmin) Console.WriteLine("Run it from an administrator command prompt.");
                return 1;
            }
        }
    }
}
