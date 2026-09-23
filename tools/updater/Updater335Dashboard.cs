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
            // Capture wired controller buttons before removing absolute layout.
            var reportButton = TakeFeatureButton("WYŚLIJ RAPORT");
            var reportTokenButton = TakeFeatureButton("TOKEN RAPORTU");
            Controls.Clear(); // Remove only the obsolete absolute-position labels from feature attachment.
            SuspendLayout();
            Font = new Font("Segoe UI", 9f);
            BackColor = UiCanvas;
            ForeColor = UiInk;
            ClientSize = new Size(1060, 555);
            MinimumSize = new Size(970, 490);
            AutoScaleMode = AutoScaleMode.Dpi;
            StartPosition = FormStartPosition.CenterScreen;
            DoubleBuffered = true;
            Text = "WoW335 Updater v" + UpdaterVersion + " • 3.3.5a / 12340";
            Icon = System.Drawing.Icon.ExtractAssociatedIcon(System.Windows.Forms.Application.ExecutablePath);

            var root = UiGrid(1, 4);
            root.Padding = new Padding(16);
            root.RowStyles.Add(new RowStyle(SizeType.Absolute, 76));
            root.RowStyles.Add(new RowStyle(SizeType.Absolute, 118));
            root.RowStyles.Add(new RowStyle(SizeType.Absolute, 184));
            root.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
            Controls.Add(root);

            var header = UiGrid(2, 1);
            header.ColumnStyles.Clear();
            header.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 52));
            header.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 48));
            var branding = UiGrid(1, 3);
            branding.RowStyles.Add(new RowStyle(SizeType.Absolute, 33));
            branding.RowStyles.Add(new RowStyle(SizeType.Absolute, 19));
            branding.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
            branding.Controls.Add(UiLabel("WoW 335", 21, UiAccent, true), 0, 0);
            branding.Controls.Add(UiLabel("Updater  " + UpdaterVersion + "   •   Wrath 3.3.5a / build 12340", 9, UiMuted), 0, 1);
            connectionBadge.Text = "GitHub: token niesprawdzony";
            connectionBadge.ForeColor = UiMuted;
            connectionBadge.Dock = DockStyle.Fill;
            branding.Controls.Add(connectionBadge, 0, 2);
            header.Controls.Add(branding, 0, 0);
            header.Controls.Add(Build335MonitorHeader(), 1, 0);
            root.Controls.Add(header, 0, 0);

            var config = UiCard("KONFIGURACJA", 3);
            var path = UiGrid(2, 1);
            path.ColumnStyles.Clear();
            path.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 94));
            path.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
                        path.Controls.Add(UiLabel("Katalog gry", 9, UiMuted), 0, 0);
            UiInput(gameDir); path.Controls.Add(gameDir, 1, 0);
            dashboardTips.SetToolTip(gameDir, "Dwuklik: wybierz katalog gry. Zmiany zapisują się automatycznie.");
            gameDir.DoubleClick += BrowseButton_Click;
            config.Controls.Add(path, 0, 1);

            var settings = UiGrid(4, 1);
            settings.ColumnStyles.Clear();
            foreach (var w in new[] { 94, 178, 112, -1 })
                settings.ColumnStyles.Add(w < 0 ? new ColumnStyle(SizeType.Percent, 100) : new ColumnStyle(SizeType.Absolute, w));
            settings.Controls.Add(UiLabel("Kanał", 9, UiMuted), 0, 0);
            UiInput(channel); settings.Controls.Add(channel, 1, 0);
            settings.Controls.Add(UiLabel("Token GitHub", 9, UiMuted), 2, 0);
            UiInput(token); settings.Controls.Add(token, 3, 0);
            token.Leave += delegate { SaveConfig(false); };
            config.Controls.Add(settings, 0, 2);
            config.Controls.Add(UiLabel("Dostęp do github12wykrzyk/335 • token tylko do odczytu (Contents + Actions), zapis DPAPI. Nie nadpisujemy realmlist.", 8.5f, UiMuted), 0, 3);
            root.Controls.Add(config, 0, 1);

            var build = UiCard("AKTUALIZACJA", 4);
            build.RowStyles.Clear();
            foreach (var h in new[] { 23f, 47f, 28f, 13f, 49f })
                build.RowStyles.Add(new RowStyle(SizeType.Absolute, h));
            var versions = UiGrid(2, 1);
            var installed = UiGrid(1, 2); installed.RowStyles.Add(new RowStyle(SizeType.Absolute, 18));
            installed.Controls.Add(UiLabel("ZAINSTALOWANO", 8.5f, UiMuted, true), 0, 0);
            localInfo.Dock = DockStyle.Fill; localInfo.ForeColor = UiInk; localInfo.AutoEllipsis = true;
            installed.Controls.Add(localInfo, 0, 1);
            versions.Controls.Add(installed, 0, 0);
            var remote = UiGrid(1, 2); remote.RowStyles.Add(new RowStyle(SizeType.Absolute, 18));
            remote.Controls.Add(UiLabel("DOSTĘPNE", 8.5f, UiMuted, true), 0, 0);
            availableBuild.Dock = DockStyle.Fill; availableBuild.ForeColor = UiInk; availableBuild.AutoEllipsis = true;
            availableBuild.Text = "Nie sprawdzono • wybierz Sprawdź";
            remote.Controls.Add(availableBuild, 0, 1);
            versions.Controls.Add(remote, 1, 0);
            build.Controls.Add(versions, 0, 1);
            status.Dock = DockStyle.Fill; status.ForeColor = UiInk; status.Font = new Font("Segoe UI", 9.5f, FontStyle.Bold);
            build.Controls.Add(status, 0, 2);
            progress.Dock = DockStyle.Fill; progress.Style = ProgressBarStyle.Marquee;
            build.Controls.Add(progress, 0, 3);
            var actions = UiGrid(3, 1);
            actions.Controls.Add(UiButton(checkButton, "Sprawdź"), 0, 0);
            actions.Controls.Add(UiButton(updateButton, "Aktualizuj"), 1, 0);
            actions.Controls.Add(UiButton(updatePlayButton, "Aktualizuj i uruchom", true), 2, 0);

            build.Controls.Add(actions, 0, 4);
            root.Controls.Add(build, 0, 2);

            // Reporting shares the session journal, not the three game actions.
            var journal = UiGrid(1, 2);
            journal.RowStyles.Add(new RowStyle(SizeType.Absolute, 32));
            journal.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
            var logHeader = UiGrid(3, 1);
            logHeader.ColumnStyles.Clear();
            logHeader.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 57));
            logHeader.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 25));
            logHeader.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 18));

            logHeader.Controls.Add(UiLabel("DZIENNIK SESJI", 9, UiAccent, true), 0, 0);
            logHeader.Controls.Add(UiButton(reportButton, "WYŚLIJ RAPORT"), 1, 0);
            logHeader.Controls.Add(UiButton(reportTokenButton, "TOKEN RAPORTU"), 2, 0);
            dashboardTips.SetToolTip(reportButton,
                "Podgląd i wysyłka diagnostyki AutoLoot/Epoch do GitHub Issues.");
            dashboardTips.SetToolTip(reportTokenButton,
                "Osobny token z uprawnieniem Issues: Read and write, zapisany przez DPAPI.");

            journal.Controls.Add(logHeader, 0, 0);
            log.Dock = DockStyle.Fill; log.BackColor = UiCanvas; log.ForeColor = UiMuted;
            log.Font = new Font("Consolas", 9f); log.BorderStyle = BorderStyle.FixedSingle;
            journal.Controls.Add(log, 0, 1);
            root.Controls.Add(journal, 0, 3);

            dashboardReady = true;
            gameDir.TextChanged += delegate { availableBuild.Text = "Katalog zmieniony • sprawdź ponownie."; lastRemote = null; };
            gameDir.Leave += delegate { SaveConfig(false); RefreshLocalState(); };
            channel.SelectedIndexChanged += delegate { availableBuild.Text = "Kanał zmieniony • sprawdź ponownie."; lastRemote = null; SaveConfig(false); };
            token.TextChanged += delegate { connectionBadge.Text = string.IsNullOrWhiteSpace(token.Text) ? "GitHub: brak tokenu" : "GitHub: token niesprawdzony"; lastRemote = null; };
            status.TextChanged += delegate { status.ForeColor = status.Text.IndexOf("błąd", StringComparison.OrdinalIgnoreCase) >= 0 ||
                status.Text.IndexOf("nie powiod", StringComparison.OrdinalIgnoreCase) >= 0 ? Color.FromArgb(255, 164, 164) : UiInk; };
            Shown += delegate { Fit335Window(); Start335Monitor(); };
            FormClosed += delegate { SaveConfig(false); dashboardTips.Dispose(); };
            ResumeLayout(true);
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
