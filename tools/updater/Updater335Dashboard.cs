using System;
using System.Collections.Generic;
using System.Drawing;
using System.Linq;
using System.Windows.Forms;

namespace WoW335Updater
{
    // Port of the compact WoW112 dashboard layout; all data and actions remain 335-only.
    internal sealed partial class MainForm
    {
        private static readonly Color UiCanvas = Color.FromArgb(15, 26, 39);
        private static readonly Color UiSurface = Color.FromArgb(24, 42, 59);
        private static readonly Color UiInk = Color.FromArgb(227, 241, 249);
        private static readonly Color UiMuted = Color.FromArgb(160, 186, 204);
        private static readonly Color UiAccent = Color.FromArgb(62, 205, 207);
        private readonly Label availableBuild = new Label();
        private readonly Label connectionBadge = new Label();
        private readonly Label modulesSummary = new Label();
        private bool advancedOpen;
        private readonly ToolTip dashboardTips = new ToolTip();
        private readonly List<Button> dashboardFeatureButtons = new List<Button>();
        private bool dashboardReady;

        private static TableLayoutPanel UiGrid(int columns, int rows)
        {
            var grid = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = columns,
                RowCount = rows, BackColor = Color.Transparent, Margin = Padding.Empty };
            for (int i = 0; i < columns; i++)
                grid.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100f / columns));
            return grid;
        }

        private static Label UiLabel(string text, float size, Color color, bool bold = false)
        {
            return new Label { Text = text, Dock = DockStyle.Fill, ForeColor = color,
                TextAlign = ContentAlignment.MiddleLeft, AutoEllipsis = true,
                Font = new Font("Segoe UI", size, bold ? FontStyle.Bold : FontStyle.Regular),
                Margin = new Padding(3, 1, 3, 1) };
        }

        private static TableLayoutPanel UiCard(string title, int rows)
        {
            var p = UiGrid(1, rows + 1);
            p.BackColor = UiSurface;
            p.Padding = new Padding(9, 5, 9, 5);
            p.Margin = new Padding(0, 0, 0, 9);
            p.RowStyles.Clear();
            p.RowStyles.Add(new RowStyle(SizeType.Absolute, 23));
            for (int i = 0; i < rows; i++)
                p.RowStyles.Add(new RowStyle(SizeType.Percent, 100f / rows));
            p.Controls.Add(UiLabel(title, 9, UiAccent, true), 0, 0);
            return p;
        }

        private static Button UiButton(Button b, string title, bool primary = false)
        {
            b.Text = title;
            b.Dock = DockStyle.Fill;
            b.Margin = new Padding(4, 3, 4, 3);
            b.FlatStyle = FlatStyle.Flat;
            b.UseVisualStyleBackColor = false;
            b.UseMnemonic = false;
            b.ForeColor = UiInk;
            b.BackColor = primary ? Color.FromArgb(17, 107, 121) : Color.FromArgb(39, 62, 82);
            b.FlatAppearance.BorderColor = primary ? UiAccent : Color.FromArgb(64, 92, 110);
            b.FlatAppearance.MouseOverBackColor = primary ? Color.FromArgb(21, 132, 146) : Color.FromArgb(52, 80, 102);
            b.Cursor = Cursors.Hand;
            return b;
        }

        private Button TakeFeatureButton(string caption)
        {
            var b = Controls.OfType<Button>().FirstOrDefault(x =>
                string.Equals(x.Text, caption, StringComparison.OrdinalIgnoreCase));
            if (b == null) throw new InvalidOperationException("Nie podłączono modułu updatera: " + caption);
            Controls.Remove(b);
            dashboardFeatureButtons.Add(b);
            return b;
        }

        private static void UiInput(Control c)
        {
            c.Dock = DockStyle.Fill;
            c.Margin = new Padding(4, 3, 4, 3);
            c.BackColor = UiCanvas;
            c.ForeColor = UiInk;
            var combo = c as ComboBox;
            if (combo != null) combo.FlatStyle = FlatStyle.Flat;
        }

        private void Build335Dashboard()
        {
            if (dashboardReady) return;
            var repair = TakeFeatureButton("VERIFY / REPAIR");
            var diagnostics = TakeFeatureButton("DIAGNOSTYKA ZIP");
            var selfUpdate = TakeFeatureButton("AKTUALIZUJ UPDATER");
            var report = TakeFeatureButton("WYŚLIJ RAPORT");
            var reportToken = TakeFeatureButton("TOKEN RAPORTU");
            var autoLootReport = TakeFeatureButton("WYŚLIJ LOG AUTOLOOT");
            Controls.Clear();
            SuspendLayout();
            Font = new Font("Segoe UI", 9f);
            BackColor = UiCanvas;
            ForeColor = UiInk;
            ClientSize = new Size(1060, 660);
            MinimumSize = new Size(960, 610);
            AutoScaleMode = AutoScaleMode.Dpi;
            StartPosition = FormStartPosition.CenterScreen;
            DoubleBuffered = true;
            Text = "WoW335 Updater v" + UpdaterVersion + " • 3.3.5a / 12340";
            Icon = System.Drawing.Icon.ExtractAssociatedIcon(System.Windows.Forms.Application.ExecutablePath);

            var root = UiGrid(1, 5);
            root.RowStyles.Clear();
            root.Padding = new Padding(12, 9, 12, 9);
            root.RowStyles.Add(new RowStyle(SizeType.Absolute, 82));
            root.RowStyles.Add(new RowStyle(SizeType.Absolute, 104));
            root.RowStyles.Add(new RowStyle(SizeType.Absolute, 166));
            var advancedHeight = new RowStyle(SizeType.Absolute, 37);
            root.RowStyles.Add(advancedHeight);
            root.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
            Controls.Add(root);

            var header = UiGrid(2, 1);
            header.ColumnStyles.Clear();
            header.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 200));
            header.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            var branding = UiGrid(1, 3);
            branding.RowStyles.Clear();
            branding.RowStyles.Add(new RowStyle(SizeType.Absolute, 32));
            branding.RowStyles.Add(new RowStyle(SizeType.Absolute, 20));
            branding.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
            branding.Controls.Add(UiLabel("WoW 335", 20, UiAccent, true), 0, 0);
            branding.Controls.Add(UiLabel("3.3.5a  •  build 12340", 8.5f, UiMuted), 0, 1);
            connectionBadge.Text = "GitHub: sprawdzanie";
            connectionBadge.ForeColor = UiMuted;
            connectionBadge.Dock = DockStyle.Fill;
            connectionBadge.AutoEllipsis = true;
            branding.Controls.Add(connectionBadge, 0, 2);
            header.Controls.Add(branding, 0, 0);
            header.Controls.Add(Build335MonitorHeader(), 1, 0);
            root.Controls.Add(header, 0, 0);

            var config = UiCard("KONFIGURACJA", 2);
            config.Padding = new Padding(9, 3, 9, 3);
            var path = UiGrid(3, 1);
            path.ColumnStyles.Clear();
            path.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 94));
            path.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            path.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 108));
            path.Controls.Add(UiLabel("Katalog gry", 9, UiMuted), 0, 0);
            UiInput(gameDir); path.Controls.Add(gameDir, 1, 0);
            path.Controls.Add(UiButton(browseButton, "Wybierz…"), 2, 0);
            config.Controls.Add(path, 0, 1);

            var settings = UiGrid(4, 1);
            settings.ColumnStyles.Clear();
            settings.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 94));
            settings.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 226));
            settings.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 114));
            settings.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            settings.Controls.Add(UiLabel("Branch gry", 9, UiMuted), 0, 0);
            UiInput(channel); settings.Controls.Add(channel, 1, 0);
            settings.Controls.Add(UiLabel("Token GitHub", 9, UiMuted), 2, 0);
            UiInput(token); settings.Controls.Add(token, 3, 0);
            dashboardTips.SetToolTip(token, "Contents: Read + Actions: Read; token jest chroniony lokalnie przez DPAPI.");
            config.Controls.Add(settings, 0, 2);
            root.Controls.Add(config, 0, 1);

            var build = UiCard("AKTUALIZACJA I MODUŁY", 5);
            build.Padding = new Padding(9, 3, 9, 3);
            build.RowStyles.Clear();
            foreach (var h in new[] { 20f, 39f, 21f, 23f, 7f, 40f })
                build.RowStyles.Add(new RowStyle(SizeType.Absolute, h));
            var versions = UiGrid(2, 1);
            var installed = UiGrid(1, 2);
            installed.RowStyles.Clear();
            installed.RowStyles.Add(new RowStyle(SizeType.Absolute, 17));
            installed.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
            installed.Controls.Add(UiLabel("ZAINSTALOWANO", 8f, UiMuted, true), 0, 0);
            localInfo.Dock = DockStyle.Fill;
            localInfo.ForeColor = UiInk;
            localInfo.AutoEllipsis = true;
            installed.Controls.Add(localInfo, 0, 1);
            versions.Controls.Add(installed, 0, 0);
            var remote = UiGrid(1, 2);
            remote.RowStyles.Clear();
            remote.RowStyles.Add(new RowStyle(SizeType.Absolute, 17));
            remote.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
            remote.Controls.Add(UiLabel("DOSTĘPNE", 8f, UiMuted, true), 0, 0);
            availableBuild.Dock = DockStyle.Fill;
            availableBuild.ForeColor = UiInk;
            availableBuild.AutoEllipsis = true;
            availableBuild.Text = "Nie sprawdzono";
            remote.Controls.Add(availableBuild, 0, 1);
            versions.Controls.Add(remote, 1, 0);
            build.Controls.Add(versions, 0, 1);

            modulesSummary.Dock = DockStyle.Fill;
            modulesSummary.ForeColor = UiMuted;
            modulesSummary.AutoEllipsis = true;
            modulesSummary.TextAlign = ContentAlignment.MiddleLeft;
            modulesSummary.Text = "Moduły: brak zainstalowanej paczki";
            build.Controls.Add(modulesSummary, 0, 2);
            status.Dock = DockStyle.Fill;
            status.ForeColor = UiInk;
            status.Font = new Font("Segoe UI", 9f, FontStyle.Bold);
            status.AutoEllipsis = true;
            build.Controls.Add(status, 0, 3);
            progress.Dock = DockStyle.Fill;
            progress.Style = ProgressBarStyle.Marquee;
            build.Controls.Add(progress, 0, 4);
            var actions = UiGrid(3, 1);
            actions.Controls.Add(UiButton(checkButton, "Sprawdź"), 0, 0);
            actions.Controls.Add(UiButton(updateButton, "Aktualizuj"), 1, 0);
            actions.Controls.Add(UiButton(updatePlayButton, "Aktualizuj i uruchom", true), 2, 0);
            build.Controls.Add(actions, 0, 5);
            root.Controls.Add(build, 0, 2);

            // Secondary actions remain functional, but occupy no space until expanded.
            var advanced = UiGrid(1, 2);
            advanced.RowStyles.Clear();
            advanced.RowStyles.Add(new RowStyle(SizeType.Absolute, 34));
            advanced.RowStyles.Add(new RowStyle(SizeType.Absolute, 0));
            var advancedDetails = UiGrid(1, 3);
            advancedDetails.RowStyles.Clear();
            advancedDetails.RowStyles.Add(new RowStyle(SizeType.Absolute, 33));
            advancedDetails.RowStyles.Add(new RowStyle(SizeType.Absolute, 33));
            advancedDetails.RowStyles.Add(new RowStyle(SizeType.Absolute, 35));
            advancedDetails.Visible = false;
            var toolsToggle = UiButton(new Button(), "Narzędzia  ▾   raporty, naprawa i kopie");
            toolsToggle.TextAlign = ContentAlignment.MiddleLeft;
            toolsToggle.Click += delegate
            {
                advancedOpen = !advancedOpen;
                advancedHeight.Height = advancedOpen ? 145 : 37;
                advanced.RowStyles[1].Height = advancedOpen ? 106 : 0;
                advancedDetails.Visible = advancedOpen;
                toolsToggle.Text = advancedOpen ? "Narzędzia  ▴   zwiń" : "Narzędzia  ▾   raporty, naprawa i kopie";
                root.PerformLayout();
            };
            advanced.Controls.Add(toolsToggle, 0, 0);
            dashboardFeatureButtons.Add(toolsToggle);

            var utilities = UiGrid(6, 1);
            var utilityButtons = new[] { repair, diagnostics, report, reportToken, selfUpdate, launchButton };
            var utilityNames = new[] { "Sprawdź / napraw", "Diagnostyka ZIP", "Wyślij raport", "Token raportu", "Aktualizuj updater", "Uruchom bez update" };
            for (int i = 0; i < utilityButtons.Length; i++)
                utilities.Controls.Add(UiButton(utilityButtons[i], utilityNames[i]), i, 0);
            advancedDetails.Controls.Add(utilities, 0, 0);

            var addonTools = UiGrid(3, 1);
            autoLootDiagInstallButton.Click += delegate { InstallAutoLootDiagnostic(); };
            dashboardFeatureButtons.Add(autoLootDiagInstallButton);
            addonTools.Controls.Add(UiButton(autoLootDiagInstallButton, "Instaluj test AutoLoot"), 0, 0);
            addonTools.Controls.Add(UiButton(autoLootReport, "Wyślij log AutoLoot"), 1, 0);
            addonTools.Controls.Add(UiButton(monitorButton, "Szczegóły GitHub"), 2, 0);
            dashboardFeatureButtons.Add(monitorButton);
            advancedDetails.Controls.Add(addonTools, 0, 1);

            var rollback = UiGrid(3, 1);
            rollback.ColumnStyles.Clear();
            rollback.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 120));
            rollback.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            rollback.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 155));
            rollback.Controls.Add(UiLabel("Kopia zapasowa", 9, UiMuted), 0, 0);
            UiInput(rollbackChoice); rollback.Controls.Add(rollbackChoice, 1, 0);
            rollback.Controls.Add(UiButton(rollbackButton, "Rollback"), 2, 0);
            advancedDetails.Controls.Add(rollback, 0, 2);
            advanced.Controls.Add(advancedDetails, 0, 1);
            root.Controls.Add(advanced, 0, 3);

            var journal = UiGrid(1, 2);
            journal.RowStyles.Clear();
            journal.RowStyles.Add(new RowStyle(SizeType.Absolute, 29));
            journal.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
            var logHeader = UiGrid(2, 1);
            logHeader.ColumnStyles.Clear();
            logHeader.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            logHeader.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 105));
            logHeader.Controls.Add(UiLabel("DZIENNIK SESJI", 8.5f, UiAccent, true), 0, 0);
            var copy = UiButton(new Button(), "Kopiuj log");
            copy.Click += delegate { try { Clipboard.SetText(log.Text); } catch (Exception ex) { Log("Błąd kopiowania: " + ex.Message); } };
            logHeader.Controls.Add(copy, 1, 0);
            journal.Controls.Add(logHeader, 0, 0);
            log.Dock = DockStyle.Fill;
            log.BackColor = UiCanvas;
            log.ForeColor = UiMuted;
            log.Font = new Font("Consolas", 8.5f);
            log.BorderStyle = BorderStyle.FixedSingle;
            journal.Controls.Add(log, 0, 1);
            root.Controls.Add(journal, 0, 4);

            dashboardReady = true;
            gameDir.TextChanged += delegate { availableBuild.Text = "Katalog zmieniony"; lastRemote = null; };
            gameDir.Leave += delegate { SaveConfig(false); RefreshLocalState(); };
            channel.SelectedIndexChanged += delegate
            {
                availableBuild.Text = "Branch zmieniony";
                lastRemote = null;
                SaveConfig(false);
            };
            token.TextChanged += delegate
            {
                connectionBadge.Text = string.IsNullOrWhiteSpace(token.Text) ? "GitHub: brak tokenu" : "GitHub: token niesprawdzony";
                lastRemote = null;
            };
            token.Leave += delegate { SaveConfig(false); };
            status.TextChanged += delegate
            {
                status.ForeColor = status.Text.IndexOf("błąd", StringComparison.OrdinalIgnoreCase) >= 0 ||
                    status.Text.IndexOf("nie powiod", StringComparison.OrdinalIgnoreCase) >= 0
                    ? Color.FromArgb(255, 164, 164) : UiInk;
            };
            Shown += delegate { Fit335Window(); Start335Monitor(); };
            FormClosed += delegate { dashboardTips.Dispose(); };
            ResumeLayout(true);
        }

        private void Update335ModuleSummary(Dictionary<string, object> installed)
        {
            if (!dashboardReady) return;
            if (installed == null)
            {
                modulesSummary.Text = "Moduły: brak zainstalowanej paczki";
                dashboardTips.SetToolTip(modulesSummary, modulesSummary.Text);
                return;
            }
            var modules = AsArray(GetValue(installed, "managed_files"))
                .Select(x => Convert.ToString(x))
                .Where(x => !string.IsNullOrWhiteSpace(x) && x.EndsWith(".dll", StringComparison.OrdinalIgnoreCase))
                .ToArray();
            modulesSummary.Text = modules.Length == 0
                ? "Moduły: brak aktywnych DLL"
                : "Moduły (" + modules.Length + "): " + string.Join("  •  ", modules);
            dashboardTips.SetToolTip(modulesSummary, modulesSummary.Text +
                "\nLista zainstalowanej paczki; nie potwierdza działania w grze.");
        }

        private void Fit335Window()
        {
            var area = Screen.FromControl(this).WorkingArea;
            if (MinimumSize.Width > area.Width || MinimumSize.Height > area.Height) MinimumSize = Size.Empty;
            Size = new Size(Math.Min(Width, area.Width), Math.Min(Height, area.Height));
            Location = new Point(Math.Max(area.Left, Math.Min(Left, area.Right - Width)),
                Math.Max(area.Top, Math.Min(Top, area.Bottom - Height)));
        }

        private void Set335DashboardBusy(bool value)
        {
            if (!dashboardReady) return;
            foreach (var b in dashboardFeatureButtons) b.Enabled = !value;
            gameDir.Enabled = !value;
            token.Enabled = !value;
        }

        private void Show335Remote(string channelName, string sha, long run)
        {
            availableBuild.Text = channelName.ToUpperInvariant() + " • " + (sha.Length > 9 ? sha.Substring(0, 9) : sha) + " • run " + run;
            dashboardTips.SetToolTip(availableBuild, availableBuild.Text);
            connectionBadge.Text = "GitHub: połączono";
            connectionBadge.ForeColor = Color.FromArgb(151, 235, 190);
        }

        private void Show335RemoteError(string message)
        {
            availableBuild.Text = message;
            dashboardTips.SetToolTip(availableBuild, message);
        }
    }
}
