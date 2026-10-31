// The installer window: a Windows 11-style light WPF window, built from XAML at runtime
// (no build step beyond csc). Status card, options, and Install / Uninstall.
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Runtime.InteropServices;
using System.Text.RegularExpressions;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Markup;
using System.Windows.Media;

namespace KLFix
{
    class MainWindow
    {
        const string ProjectUrl = "https://github.com/carbiecarbie/KL_Fix";

        const string Styles = @"
<ResourceDictionary xmlns='http://schemas.microsoft.com/winfx/2006/xaml/presentation'
                    xmlns:x='http://schemas.microsoft.com/winfx/2006/xaml'>
  <Style TargetType='Button'>
    <Setter Property='Background' Value='#FFFFFF'/>
    <Setter Property='BorderBrush' Value='#D1D1D1'/>
    <Setter Property='BorderThickness' Value='1'/>
    <Setter Property='Foreground' Value='#1A1A1A'/>
    <Setter Property='Padding' Value='16,8'/>
    <Setter Property='Cursor' Value='Hand'/>
    <Setter Property='FocusVisualStyle' Value='{x:Null}'/>
    <Setter Property='Template'>
      <Setter.Value>
        <ControlTemplate TargetType='Button'>
          <Border Background='{TemplateBinding Background}' BorderBrush='{TemplateBinding BorderBrush}'
                  BorderThickness='{TemplateBinding BorderThickness}' CornerRadius='6' Padding='{TemplateBinding Padding}'>
            <ContentPresenter HorizontalAlignment='Center' VerticalAlignment='Center'/>
          </Border>
        </ControlTemplate>
      </Setter.Value>
    </Setter>
    <Style.Triggers>
      <Trigger Property='IsMouseOver' Value='True'><Setter Property='Background' Value='#F5F5F5'/></Trigger>
      <Trigger Property='IsPressed' Value='True'><Setter Property='Background' Value='#EBEBEB'/></Trigger>
      <Trigger Property='IsKeyboardFocused' Value='True'><Setter Property='BorderBrush' Value='#005FB8'/><Setter Property='BorderThickness' Value='2'/></Trigger>
      <Trigger Property='IsEnabled' Value='False'><Setter Property='Opacity' Value='0.5'/></Trigger>
    </Style.Triggers>
  </Style>
  <Style x:Key='Accent' TargetType='Button' BasedOn='{StaticResource {x:Type Button}}'>
    <Setter Property='Background' Value='#005FB8'/>
    <Setter Property='BorderBrush' Value='#005FB8'/>
    <Setter Property='Foreground' Value='#FFFFFF'/>
    <Setter Property='FontWeight' Value='SemiBold'/>
    <Style.Triggers>
      <Trigger Property='IsMouseOver' Value='True'><Setter Property='Background' Value='#1A6FC0'/></Trigger>
      <Trigger Property='IsPressed' Value='True'><Setter Property='Background' Value='#004C94'/></Trigger>
      <Trigger Property='IsKeyboardFocused' Value='True'><Setter Property='BorderBrush' Value='#003A70'/><Setter Property='BorderThickness' Value='2'/></Trigger>
    </Style.Triggers>
  </Style>
  <Style x:Key='Link' TargetType='Button'>
    <Setter Property='Foreground' Value='#005FB8'/>
    <Setter Property='Cursor' Value='Hand'/>
    <Setter Property='Padding' Value='6,2'/>
    <Setter Property='Template'>
      <Setter.Value>
        <ControlTemplate TargetType='Button'>
          <Border Background='Transparent' Padding='{TemplateBinding Padding}'><ContentPresenter/></Border>
        </ControlTemplate>
      </Setter.Value>
    </Setter>
    <Style.Triggers>
      <Trigger Property='IsMouseOver' Value='True'><Setter Property='Foreground' Value='#003A70'/></Trigger>
      <Trigger Property='IsEnabled' Value='False'><Setter Property='Opacity' Value='0.5'/></Trigger>
    </Style.Triggers>
  </Style>
  <Style x:Key='Card' TargetType='Border'>
    <Setter Property='Background' Value='#FFFFFF'/>
    <Setter Property='BorderBrush' Value='#E5E5E5'/>
    <Setter Property='BorderThickness' Value='1'/>
    <Setter Property='CornerRadius' Value='8'/>
  </Style>
  <Style x:Key='Note' TargetType='TextBlock'>
    <Setter Property='Foreground' Value='#5F5F5F'/>
    <Setter Property='FontSize' Value='13'/>
    <Setter Property='TextWrapping' Value='Wrap'/>
  </Style>
  <Style TargetType='ComboBox'>
    <Setter Property='Padding' Value='8,5'/>
  </Style>
</ResourceDictionary>";

        const string Layout = @"
<Window xmlns='http://schemas.microsoft.com/winfx/2006/xaml/presentation'
        xmlns:x='http://schemas.microsoft.com/winfx/2006/xaml'
        Title='Kane &amp; Lynch: Dead Men - KL Fix' Width='640' SizeToContent='Height' ResizeMode='CanMinimize'
        WindowStartupLocation='CenterScreen' Background='#F3F3F3' FontFamily='Segoe UI' FontSize='14'
        UseLayoutRounding='True' SnapsToDevicePixels='True'>
  <!--STYLES-->
  <StackPanel Margin='28,22,28,20'>
    <TextBlock Text='Kane &amp; Lynch: Dead Men' FontSize='14' Foreground='#5F5F5F'/>
    <StackPanel Orientation='Horizontal'>
      <TextBlock Text='KL Fix' FontSize='28' FontWeight='SemiBold' Foreground='#1A1A1A'/>
      <TextBlock x:Name='Version' FontSize='15' Foreground='#5F5F5F' VerticalAlignment='Bottom' Margin='10,0,0,6'/>
    </StackPanel>
    <TextBlock Style='{StaticResource Note}' Margin='0,4,0,0'
               Text='Removes the hitch every 2 seconds, smooths frame timing, scales the HUD, subtitles and menus at high resolutions, and removes the need for Games for Windows LIVE.'/>
    <Border Style='{StaticResource Card}' Margin='0,16,0,0' Padding='18,6'>
      <StackPanel x:Name='Status'/>
    </Border>
    <Border x:Name='Options' Style='{StaticResource Card}' Margin='0,12,0,0' Padding='18,14'>
      <StackPanel>
        <StackPanel x:Name='GfwlPanel' Margin='0,0,0,16'>
          <CheckBox x:Name='ReplaceGfwl' IsChecked='True' VerticalContentAlignment='Center'>
            <TextBlock Text='Replace Games for Windows LIVE with an offline version (recommended)' FontWeight='SemiBold' TextWrapping='Wrap'/>
          </CheckBox>
          <TextBlock Style='{StaticResource Note}' Margin='22,4,0,0'
                     Text='GFWL no longer needs to be installed or signed in. Single-player and local split-screen co-op work; online multiplayer is not available. Progress saved under a GFWL account may not carry over.'/>
        </StackPanel>
        <TextBlock Text='Interface size' FontWeight='SemiBold'/>
        <TextBlock x:Name='SizeNote' Style='{StaticResource Note}' Margin='0,2,0,6'/>
        <Grid x:Name='Sizes'>
          <Grid.ColumnDefinitions><ColumnDefinition Width='110'/><ColumnDefinition/></Grid.ColumnDefinitions>
        </Grid>
      </StackPanel>
    </Border>
    <Button x:Name='Install' Style='{StaticResource Accent}' Content='Install' FontSize='17' Height='50' Margin='0,18,0,0'/>
    <Grid Margin='0,10,0,0'>
      <Grid.ColumnDefinitions><ColumnDefinition/><ColumnDefinition Width='10'/><ColumnDefinition/></Grid.ColumnDefinitions>
      <Button x:Name='Uninstall' Grid.Column='0' Content='Uninstall'/>
      <Button x:Name='ChangeFolder' Grid.Column='2' Content='Change game folder'/>
    </Grid>
    <StackPanel Orientation='Horizontal' HorizontalAlignment='Center' Margin='0,14,0,0'>
      <Button x:Name='OpenFolder' Style='{StaticResource Link}' Content='Open game folder'/>
      <TextBlock Text='&#x00B7;' Foreground='#8A8A8A' VerticalAlignment='Center'/>
      <Button x:Name='OpenLog' Style='{StaticResource Link}' Content='Open log'/>
      <TextBlock Text='&#x00B7;' Foreground='#8A8A8A' VerticalAlignment='Center'/>
      <Button x:Name='Project' Style='{StaticResource Link}' Content='Project page'/>
    </StackPanel>
    <TextBlock x:Name='Notice' Style='{StaticResource Note}' TextAlignment='Center' HorizontalAlignment='Center' Margin='0,8,0,0'/>
  </StackPanel>
</Window>";

        // Interface size choices: ini value, label. "auto" is first.
        static readonly string[][] ScaleChoices = {
            new[] { "1", "1× (original size)" }, new[] { "1.25", "1.25×" }, new[] { "1.5", "1.5×" },
            new[] { "1.75", "1.75×" }, new[] { "2", "2×" }, new[] { "2.5", "2.5×" }, new[] { "3", "3×" },
        };
        static readonly string[] SizeKeys = { "HudScale", "SubtitleScale", "MenuScale" };
        static readonly string[] SizeLabels = { "HUD", "Subtitles", "Menus" };

        public Window Window;
        string folder;
        GameState st;
        string loadError;
        bool loading;
        readonly ComboBox[] sizeBoxes = new ComboBox[3];
        readonly Dictionary<string, string> pending = new Dictionary<string, string>();

        public MainWindow(string gameFolder)
        {
            Window = (Window)XamlReader.Parse(Layout.Replace("<!--STYLES-->", "<Window.Resources>" + Styles + "</Window.Resources>"));
            Find<TextBlock>("Version").Text = GameState.Version;
            folder = gameFolder;
            BuildSizeRows();
            Find<Button>("Install").Click += delegate { Run(true); };
            Find<Button>("Uninstall").Click += delegate { Run(false); };
            Find<Button>("ChangeFolder").Click += delegate { ChooseFolder(); };
            Find<Button>("OpenFolder").Click += delegate { if (folder != null) Shell("explorer.exe", Quote(folder)); };
            Find<Button>("OpenLog").Click += delegate { if (LogFile() != null) Shell(LogFile(), null); };
            Find<Button>("Project").Click += delegate { Shell(ProjectUrl, null); };
            Window.Activated += delegate { if (!loading) Refresh(); };
            Refresh();
            if (folder == null) Window.Loaded += delegate { ChooseFolder(); };
        }

        string LogFile()
        {
            if (st == null) return null;
            if (File.Exists(st.Log)) return st.Log;
            return File.Exists(st.OldLog) ? st.OldLog : null;
        }

        T Find<T>(string name) where T : class { return Window.FindName(name) as T; }

        static string Quote(string s) { return "\"" + s + "\""; }

        static void Shell(string file, string args)
        {
            try { Process.Start(new ProcessStartInfo(file, args ?? "") { UseShellExecute = true }); }
            catch (Exception) { }
        }

        // ---- Interface size rows -------------------------------------------------------

        void BuildSizeRows()
        {
            var grid = Find<Grid>("Sizes");
            for (int i = 0; i < 3; i++)
            {
                grid.RowDefinitions.Add(new RowDefinition());
                var label = new TextBlock { Text = SizeLabels[i], VerticalAlignment = VerticalAlignment.Center, Foreground = Brush("#5F5F5F") };
                var box = new ComboBox { Margin = new Thickness(0, 3, 0, 3) };
                int index = i;
                box.SelectionChanged += delegate { SizeChanged(index); };
                Grid.SetRow(label, i);
                Grid.SetRow(box, i);
                Grid.SetColumn(box, 1);
                grid.Children.Add(label);
                grid.Children.Add(box);
                sizeBoxes[i] = box;
            }
        }

        [DllImport("user32.dll")]
        static extern int GetSystemMetrics(int index);

        // The game's resolution (main.ini), else the screen's, as the DLL computes it.
        static int GameHeight()
        {
            int h = GetSystemMetrics(1);
            try
            {
                string ini = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), @"kaneandlynch\main.ini");
                if (File.Exists(ini))
                    foreach (string line in File.ReadAllLines(ini))
                    {
                        var m = Regex.Match(line, @"^\s*Resolution\s+(\d+)x(\d+)");
                        if (m.Success) h = int.Parse(m.Groups[2].Value);
                    }
            }
            catch (Exception) { }
            return h;
        }

        void FillSizes()
        {
            int height = GameHeight();
            double auto = Math.Max(1.0, height / 1080.0);
            Find<TextBlock>("SizeNote").Text = string.Format(CultureInfo.InvariantCulture,
                "Auto keeps them the size they are at 1080p: {0:0.##}× at your {1}p. Changes apply the next time the game starts.", auto, height);
            string[] autoLabels = {
                string.Format(CultureInfo.InvariantCulture, "Auto ({0:0.##}×, recommended)", auto),
                "Auto (same as HUD)",
                string.Format(CultureInfo.InvariantCulture, "Auto ({0:0.##}×, recommended)", auto),
            };
            for (int i = 0; i < 3; i++)
            {
                var box = sizeBoxes[i];
                box.Items.Clear();
                box.Items.Add(new ComboBoxItem { Content = autoLabels[i], Tag = "auto" });
                foreach (var c in ScaleChoices)
                    box.Items.Add(new ComboBoxItem { Content = (i == 2 && c[0] == "1") ? "Off (original size)" : c[1], Tag = c[0] });
                string value;
                if (!pending.TryGetValue(SizeKeys[i], out value))
                    value = st != null && File.Exists(st.SettingsFile) ? Installer.ReadSetting(st, SizeKeys[i]) : "auto";
                ComboBoxItem chosen = null;
                foreach (ComboBoxItem item in box.Items)
                    if (Same((string)item.Tag, value)) chosen = item;
                if (chosen == null)
                {
                    chosen = new ComboBoxItem { Content = "Custom (" + value + ")", Tag = value };
                    box.Items.Add(chosen);
                }
                box.SelectedItem = chosen;
            }
        }

        static bool Same(string a, string b)
        {
            double x, y;
            if (double.TryParse(a, NumberStyles.Float, CultureInfo.InvariantCulture, out x) &&
                double.TryParse(b, NumberStyles.Float, CultureInfo.InvariantCulture, out y)) return Math.Abs(x - y) < 1e-6;
            return string.Equals(a, b, StringComparison.OrdinalIgnoreCase);
        }

        void SizeChanged(int i)
        {
            if (loading) return;
            var item = sizeBoxes[i].SelectedItem as ComboBoxItem;
            if (item == null) return;
            string value = (string)item.Tag;
            if (st == null || !st.DllOurs)
            {
                pending[SizeKeys[i]] = value;  // written when the fix is installed
                return;
            }
            try
            {
                Installer.WriteSetting(st, SizeKeys[i], value);
                Say("Saved. It applies the next time the game starts.", "#0F7B0F");
            }
            catch (FixException e) { Fail(e); }
        }

        // ---- State and status card -------------------------------------------------------

        void Refresh()
        {
            loading = true;
            try
            {
                st = null;
                loadError = null;
                if (folder != null)
                {
                    try { st = GameState.Load(folder); }
                    catch (FixException e) { loadError = e.Message; }
                }
                FillStatus();
                FillSizes();
                UpdateButtons();
            }
            finally { loading = false; }
        }

        static Brush Brush(string color) { return (Brush)new BrushConverter().ConvertFromString(color); }

        void FillStatus()
        {
            var rows = new List<string[]>();  // level, title, value, detail
            bool running = Installer.GameRunning();
            if (folder == null)
                rows.Add(new[] { "warn", "Game", "Not found", "Choose the folder where kaneandlynch.exe is." });
            else if (st == null)
                rows.Add(new[] { "warn", "Game", "Could not be read", loadError + "\n" + folder });
            else
            {
                string edition = st.Edition == Edition.Gog ? "GOG version" : st.Edition == Edition.Steam ? "Steam version" : "Retail version";
                if (!st.ExeSupported && !st.Installed)
                    rows.Add(new[] { "warn", "Game", "Unsupported version", "This kaneandlynch.exe is not a version the fix knows.\n" + folder });
                else rows.Add(new[] { "ok", "Game", edition, folder });

                if (st.DllOld) rows.Add(new[] { "warn", "KL Fix", "Older version installed", "Stutter Fix 2.x. Click Update to install " + GameState.Version + "; your settings are kept." });
                else if (st.DllOurs) rows.Add(new[] { "ok", "KL Fix", "Installed", "Hitch fix, smooth frame timing and interface scaling are active." });
                else rows.Add(new[] { "info", "KL Fix", "Not installed", "" });
                if (st.ExePatched)
                    rows.Add(new[] { "warn", "Game exe", "Modified by version 1.0", st.V1Backup != null
                        ? "Installing restores the original kaneandlynch.exe."
                        : "No backup found. On Steam, use \"Verify integrity of game files\"." });

                if (st.XliveOurs) rows.Add(new[] { "ok", "GFWL", "Not needed", "Replaced by this fix's offline xlive.dll." });
                else if (st.XliveForeign)
                    rows.Add(new[] { "ok", "GFWL", "Not needed", st.Edition == Edition.Gog
                        ? "The GOG version has its own offline xlive.dll."
                        : "The game folder has its own xlive.dll, which is kept." });
                else rows.Add(new[] { "info", "GFWL", "Required by this version", "Games for Windows LIVE must be installed. The fix can replace it." });

                if (st.DllForeign) rows.Add(new[] { "info", "Other mod", "dinput8.dll", "Kept as dinput8_next.dll; it still loads." });
                else if (st.DllOurs && st.ChainPresent) rows.Add(new[] { "ok", "Other mod", "Loaded together", "dinput8_next.dll" });
            }
            if (running) rows.Add(new[] { "warn", "Game", "Running", "Close it to install or uninstall." });

            var panel = Find<StackPanel>("Status");
            panel.Children.Clear();
            var glyphs = new Dictionary<string, string> { { "ok", "" }, { "warn", "" }, { "info", "" } };
            var colors = new Dictionary<string, string> { { "ok", "#0F7B0F" }, { "warn", "#9D5D00" }, { "info", "#5F5F5F" } };
            bool first = true;
            foreach (var row in rows)
            {
                var grid = new Grid { Margin = new Thickness(0, 10, 0, 10) };
                grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(30) });
                grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(110) });
                grid.ColumnDefinitions.Add(new ColumnDefinition());
                var icon = new TextBlock { Text = glyphs[row[0]], FontFamily = new FontFamily("Segoe MDL2 Assets"), FontSize = 16,
                                           Foreground = Brush(colors[row[0]]), Margin = new Thickness(0, 2, 0, 0) };
                var title = new TextBlock { Text = row[1], Foreground = Brush("#5F5F5F") };
                var values = new StackPanel();
                values.Children.Add(new TextBlock { Text = row[2], FontWeight = FontWeights.SemiBold, Foreground = Brush("#1A1A1A"), TextWrapping = TextWrapping.Wrap });
                if (!string.IsNullOrEmpty(row[3]))
                    values.Children.Add(new TextBlock { Text = row[3], Foreground = Brush("#5F5F5F"), FontSize = 12.5,
                                                        TextWrapping = TextWrapping.Wrap, Margin = new Thickness(0, 2, 0, 0) });
                Grid.SetColumn(title, 1);
                Grid.SetColumn(values, 2);
                grid.Children.Add(icon);
                grid.Children.Add(title);
                grid.Children.Add(values);
                if (!first) panel.Children.Add(new Border { Height = 1, Background = Brush("#EDEDED") });
                panel.Children.Add(grid);
                first = false;
            }
        }

        void UpdateButtons()
        {
            bool running = Installer.GameRunning();
            bool usable = st != null && (st.ExeSupported || st.Installed);
            var install = Find<Button>("Install");
            install.Content = st == null || !st.DllOurs ? "Install" : st.DllOld ? "Update" : "Reinstall";
            install.IsEnabled = usable && !running;
            Find<Button>("Uninstall").IsEnabled = st != null && (st.Installed || st.ExePatched) && !running;
            Find<Button>("OpenFolder").IsEnabled = folder != null;
            Find<Button>("OpenLog").IsEnabled = LogFile() != null;
            Find<Border>("Options").IsEnabled = usable;
            Find<StackPanel>("GfwlPanel").Visibility = st != null && st.GfwlChoice ? Visibility.Visible : Visibility.Collapsed;
        }

        void Say(string text, string color)
        {
            var notice = Find<TextBlock>("Notice");
            notice.Text = text;
            notice.Foreground = Brush(color);
        }

        // ---- Actions --------------------------------------------------------------------

        void ChooseFolder()
        {
            var dialog = new Microsoft.Win32.OpenFileDialog
            {
                Title = "Select kaneandlynch.exe in the game folder",
                Filter = "Kane & Lynch: Dead Men|kaneandlynch.exe",
                CheckFileExists = true,
            };
            if (folder != null) dialog.InitialDirectory = folder;
            if (dialog.ShowDialog(Window) != true) return;
            string found = Installer.Resolve(dialog.FileName);
            if (found == null) return;
            folder = found;
            pending.Clear();
            Say("", "#5F5F5F");
            Refresh();
        }

        void Run(bool install)
        {
            if (st == null) return;
            if (!install && MessageBox.Show(Window, "Remove KL Fix from this game?" +
                    (st.XliveOurs && !File.Exists(st.NogfwlBackup) ? "\n\nThe game will need Games for Windows LIVE again." : ""),
                    "Uninstall", MessageBoxButton.YesNo, MessageBoxImage.Question) != MessageBoxResult.Yes) return;
            try
            {
                List<string> log;
                if (install)
                {
                    bool keepGfwl = st.GfwlChoice && Find<CheckBox>("ReplaceGfwl").IsChecked != true;
                    log = Installer.Install(st, keepGfwl);
                    foreach (var kv in pending) Installer.WriteSetting(st, kv.Key, kv.Value);
                    pending.Clear();
                    log.Insert(0, "Installed. Start the game as usual.");
                    log.Add("Connect controllers before starting the game.");
                }
                else
                {
                    log = Installer.Uninstall(st);
                    log.Insert(0, "KL Fix was removed.");
                }
                Refresh();
                Say(string.Join("\n", log), "#0F7B0F");
            }
            catch (FixException e)
            {
                Refresh();
                Fail(e);
            }
        }

        void Fail(FixException e)
        {
            Say(e.Message, "#C42B1C");
            if (!e.NeedsAdmin) return;
            if (MessageBox.Show(Window, e.Message + "\n\nRestart the installer as administrator?", "KL Fix",
                    MessageBoxButton.YesNo, MessageBoxImage.Warning) != MessageBoxResult.Yes) return;
            try
            {
                Process.Start(new ProcessStartInfo(Process.GetCurrentProcess().MainModule.FileName, Quote(folder))
                              { UseShellExecute = true, Verb = "runas" });
                Window.Close();
            }
            catch (Win32Exception) { }  // the user declined the prompt
        }
    }
}
